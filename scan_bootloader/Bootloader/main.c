/*
 * ============================================================================
 * 文件: main.c
 * 功能: SCAN Bootloader 主逻辑 —— 上电自检 → 读升级状态 → 升级/回滚/跳转。
 *
 * 整体流程:
 *   1. 初始化时钟(HSEx9→72MHz)、SysTick、看门狗、串口、SPI、I2C、CRC 表;
 *   2. 读取 W24C02 中的 Boot 状态机字节;
 *   3. 按状态机分支处理:
 *        - 0xAA(STAGED) : 有新固件 → 备份旧App → 烧写新固件 → 跳转;
 *        - 0xCC(FLASHED): App 待确认 → 计数, 超限或 App 无效则回滚;
 *        - 0x5A(ENTER_BL): App 请求进入控制台;
 *        - 0xFF(NORMAL) : 直接跳转 App;
 *   4. 没有有效 App 时进入串口控制台(可手动升级/恢复)。
 *
 * 健壮性:
 *   - 全程喂独立看门狗, 任何死循环最多 5 秒自动复位;
 *   - 所有外设操作带超时, 总线故障不阻塞;
 *   - 烧写前备份、烧写后逐块验证 + 整体 CRC, 断电可续。
 * ============================================================================
 */
#include "bl_config.h"
#include "bl_delay.h"
#include "bl_uart.h"
#include "bl_spi.h"
#include "bl_i2c.h"
#include "bl_crc32.h"
#include "bl_flash.h"
#include "bl_w25q32.h"
#include "bl_w24c02.h"
#include "bl_iap.h"
#include "stm32f1xx.h"
#include <string.h>

/* ============================ 看门狗 =====================================
 * IWDG 配置见 bl_config.h: LSI/64 ≈ 1.6ms 计数, 重装 3125 ≈ 5 秒超时。
 * 注意: IWDG 一旦启动无法软件关闭, 复位后仍在运行, 所以 App 也必须喂狗。
 * ========================================================================= */
static void bl_wdg_init(void)
{
    IWDG->KR = 0x5555U;                       /* 密钥: 允许访问 PR/RLR   */
    IWDG->PR = BL_WDG_PRESCALER;              /* 预分频 /64              */
    IWDG->RLR = BL_WDG_RELOAD;                /* 重装值 ≈ 5 秒           */
    IWDG->KR = 0xAAAAU;                       /* 密钥: 重载计数          */
    IWDG->KR = 0xCCCCU;                       /* 密钥: 启动看门狗        */
}

/* 喂狗: 只要写入 0xAAAA 就重载计数器 */
static void bl_wdg_feed(void)
{
    IWDG->KR = 0xAAAAU;
}

/* ============================ 时钟初始化 =================================
 * 与 App 的 SystemClock_Config 一致: HSE 8MHz → PLL x9 → 72MHz 系统时钟。
 * APB1 分频 /2 => 36MHz, APB2 不分频 => 72MHz。
 * 注意先把 Flash 等待周期设为 2(72MHz 下必须), 再切 PLL, 否则取指会出错。
 * ========================================================================= */
static void bl_system_init(void)
{
    /* 1. Flash 预取 + 2 个等待周期(72MHz 要求) */
    FLASH->ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    /* 2. 打开 HSE(外部 8MHz 晶振)并等待就绪 */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
    }

    /* 3. 清空并重设 PLL/分频相关位:
     *      PLLSRC = 1     (PLL 时钟源选 HSE)
     *      PLLMUL = 9     (倍频 x9 => 8MHz*9 = 72MHz)
     *      PPRE1  = /2    (APB1 36MHz)
     *      PPRE2/HPRE = /1 */
    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL |
                   RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_HPRE);
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2;

    /* 4. 打开 PLL 并等待锁相环就绪 */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    /* 5. 系统时钟切换到 PLL, 等待切换完成 */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    /* 6. 更新全局时钟变量(串口波特率等计算依赖它) */
    SystemCoreClock = 72000000UL;
}

/* ============================ EEPROM 读写助手 ============================
 * 封装状态机字节/失败计数的读写, 失败时静默处理(读失败按 NORMAL 处理)。
 * ========================================================================= */

/* 读取 Boot 状态与失败计数; 返回 1=EEPROM 正常, 0=读取失败 */
static uint8_t ee_read(uint8_t *state, uint8_t *cnt)
{
    uint8_t s = BOOT_STATE_NORMAL;
    uint8_t c = 0U;

    if ((state == NULL) || (cnt == NULL))
    {
        return 0U;
    }
    if (!bl_w24c02_read_byte(EE_BOOT_STATE, &s))
    {
        return 0U;
    }
    (void)bl_w24c02_read_byte(EE_BOOT_ATTEMPT_CNT, &c);
    *state = s;
    *cnt = c;
    return 1U;
}

/* 写状态机字节(忽略失败) */
static void ee_write_state(uint8_t state)
{
    (void)bl_w24c02_write_byte(EE_BOOT_STATE, state);
}

/* 写失败计数(忽略失败) */
static void ee_write_cnt(uint8_t cnt)
{
    (void)bl_w24c02_write_byte(EE_BOOT_ATTEMPT_CNT, cnt);
}

/* 写入新固件的版本号与长度(供 App 启动后读取/上报) */
static void ee_write_new_version(const OtaImageHeader *hdr)
{
    (void)bl_w24c02_write_byte(EE_NEW_VER_MAJOR, hdr->ver_major);
    (void)bl_w24c02_write_byte(EE_NEW_VER_MINOR, hdr->ver_minor);
    (void)bl_w24c02_write_byte(EE_FW_LEN_H, (uint8_t)(hdr->length >> 8));
    (void)bl_w24c02_write_byte(EE_FW_LEN_L, (uint8_t)(hdr->length & 0xFFU));
}

/* ============================ 镜像读取/校验 ============================= */

/* 从 W25Q32 槽位读取 16 字节镜像头 */
static uint8_t read_header(uint32_t slot, OtaImageHeader *hdr)
{
    if (hdr == NULL)
    {
        return 0U;
    }
    return bl_w25q32_read(slot, (uint8_t *)hdr, OTA_HEADER_SIZE);
}

/*
 * 校验槽位镜像:
 *   1. 魔数匹配(magic 区分暂存区/备份区);
 *   2. payload 长度合法: >=0x100 且 <=48KB 且 4 字节对齐(Flash 半字编程要求);
 *   3. 对 W25Q32 上的 payload 重新计算 CRC-32, 与头部 crc32 比对。
 * 返回 1=镜像完整有效。
 */
static uint8_t validate_image(uint32_t slot, uint32_t magic, const OtaImageHeader *hdr)
{
    uint32_t crc;

    if ((hdr == NULL) || (hdr->magic != magic))
    {
        return 0U;
    }
    if ((hdr->length < 0x100U) || (hdr->length > APP_MAX_SIZE) ||
        ((hdr->length & 0x3U) != 0U))
    {
        return 0U;
    }
    crc = bl_w25q32_crc(slot + OTA_HEADER_SIZE, hdr->length);
    return (crc == hdr->crc32) ? 1U : 0U;
}

/* ======================== 烧写引擎(通用) ================================
 * 把 W25Q32 槽位上的 payload 烧写到内部 Flash App 区:
 *   擦除全部 App 区 → 分页读取 W25Q32 → 编程 → 逐页比对 → 整体 CRC 比对。
 * 任何一步失败立即返回 0, 调用方负责回滚。
 * ========================================================================= */
static uint8_t flash_payload(uint32_t slot, const OtaImageHeader *hdr)
{
    uint8_t buf[W25Q_PAGE_SIZE];
    uint32_t done = 0U;
    uint32_t crc;

    /* 1. 擦除整个 App 区(48 个 1KB 扇区) */
    if (!bl_flash_erase_range(APP_BASE, APP_MAX_SIZE))
    {
        bl_uart_write("  erase fail\r\n");
        return 0U;
    }
    bl_wdg_feed();

    /* 2. 分页: 读 256B → 写 Flash → 逐字节比对, 边写边验证 */
    while (done < hdr->length)
    {
        uint32_t chunk = hdr->length - done;

        if (chunk > W25Q_PAGE_SIZE)
        {
            chunk = W25Q_PAGE_SIZE;
        }
        if (!bl_w25q32_read(slot + OTA_HEADER_SIZE + done, buf, chunk))
        {
            return 0U;
        }
        if (!bl_flash_write(APP_BASE + done, buf, chunk))
        {
            return 0U;
        }
        if (!bl_flash_verify(APP_BASE + done, buf, chunk))
        {
            return 0U;
        }
        bl_wdg_feed();
        done += chunk;
    }

    /* 3. 整体 CRC 比对(能发现逐块比对漏掉的一致性错误) */
    if (!bl_flash_crc(APP_BASE, hdr->length, &crc))
    {
        return 0U;
    }
    if (crc != hdr->crc32)
    {
        bl_uart_write("  final crc fail\r\n");
        return 0U;
    }
    return 1U;
}

/* ============================ 备份 / 恢复 ================================ */

/*
 * 把当前 App 区(48KB)完整备份到 W25Q32 备份槽。
 * - 若备份槽已存在"与当前 App 一致"的备份(CRC 相同), 直接跳过,
 *   这覆盖了"烧写中途断电, 重新上电继续"的场景;
 * - 备份内容 = "SCAB" 头 + 48KB 原始数据, 备份完成后读回算 CRC 验证。
 */
static uint8_t backup_current_app(void)
{
    OtaImageHeader hdr;
    uint8_t buf[W25Q_PAGE_SIZE];
    uint32_t done = 0U;
    uint32_t cur_crc;
    uint32_t ok_crc;

    bl_uart_write("[BL] backup current app -> W25Q32\r\n");

    /* 若已有有效备份且与当前 App 区内容一致, 无需重复备份 */
    if (read_header(W25Q_BACKUP_ADDR, &hdr))
    {
        if ((hdr.magic == OTA_MAGIC_BACKUP) && (hdr.length == APP_MAX_SIZE))
        {
            ok_crc = bl_w25q32_crc(W25Q_BACKUP_ADDR + OTA_HEADER_SIZE, APP_MAX_SIZE);
            if ((ok_crc == hdr.crc32) && bl_flash_crc(APP_BASE, APP_MAX_SIZE, &cur_crc) &&
                (cur_crc == hdr.crc32))
            {
                bl_uart_write("  already backed up\r\n");
                return 1U;
            }
        }
    }

    /* 1. 擦除备份区(12 个扇区) */
    if (!bl_w25q32_erase_range(W25Q_BACKUP_ADDR, APP_MAX_SIZE + OTA_HEADER_SIZE))
    {
        return 0U;
    }
    bl_wdg_feed();

    /* 2. 计算当前 App 区整体 CRC, 写入备份头 */
    if (!bl_flash_crc(APP_BASE, APP_MAX_SIZE, &cur_crc))
    {
        return 0U;
    }
    hdr.magic = OTA_MAGIC_BACKUP;
    hdr.ver_major = 0U;
    hdr.ver_minor = 0U;
    hdr.reserved = 0U;
    hdr.length = APP_MAX_SIZE;
    hdr.crc32 = cur_crc;
    if (!bl_w25q32_write(W25Q_BACKUP_ADDR, (const uint8_t *)&hdr, OTA_HEADER_SIZE))
    {
        return 0U;
    }

    /* 3. 分页把内部 Flash 数据搬到 W25Q32 备份区 */
    while (done < APP_MAX_SIZE)
    {
        uint32_t chunk = APP_MAX_SIZE - done;

        if (chunk > W25Q_PAGE_SIZE)
        {
            chunk = W25Q_PAGE_SIZE;
        }
        memcpy(buf, (const void *)(APP_BASE + done), chunk);
        if (!bl_w25q32_write(W25Q_BACKUP_ADDR + OTA_HEADER_SIZE + done, buf, chunk))
        {
            return 0U;
        }
        bl_wdg_feed();
        done += chunk;
    }

    /* 4. 读回备份区整体 CRC 验证 */
    ok_crc = bl_w25q32_crc(W25Q_BACKUP_ADDR + OTA_HEADER_SIZE, APP_MAX_SIZE);
    if (ok_crc != cur_crc)
    {
        bl_uart_write("  backup verify fail\r\n");
        return 0U;
    }
    bl_uart_write("  backup OK\r\n");
    return 1U;
}

/*
 * 从备份槽恢复 App(回滚): 校验备份头 → 复用烧写引擎写回内部 Flash。
 * 返回 1=恢复成功。
 */
static uint8_t restore_from_backup(void)
{
    OtaImageHeader hdr;

    bl_uart_write("[BL] restore previous app from W25Q32\r\n");
    if (!read_header(W25Q_BACKUP_ADDR, &hdr))
    {
        return 0U;
    }
    if (hdr.magic != OTA_MAGIC_BACKUP)
    {
        return 0U;
    }
    /* 备份镜像必须先通过完整校验, 防止把坏数据写回内部 Flash */
    if (!validate_image(W25Q_BACKUP_ADDR, OTA_MAGIC_BACKUP, &hdr))
    {
        return 0U;
    }
    if (!flash_payload(W25Q_BACKUP_ADDR, &hdr))
    {
        return 0U;
    }
    bl_uart_write("  restore OK\r\n");
    return 1U;
}

/* ============================ 升级流程 =================================== */

/*
 * 执行一次升级(STAGED 状态):
 *   校验暂存镜像 → 备份当前 App → 烧写新 App → 写状态 FLASHED + 新版本号。
 * 备份失败则中止升级(旧 App 保持不动), 烧写失败则立即尝试回滚。
 */
static void do_update(void)
{
    OtaImageHeader hdr;

    bl_uart_write("[BL] update: staged image found\r\n");
    if (!read_header(W25Q_STAGING_ADDR, &hdr))
    {
        bl_uart_write("  staging read fail\r\n");
    }
    else if (!validate_image(W25Q_STAGING_ADDR, OTA_MAGIC_STAGING, &hdr))
    {
        bl_uart_write("  staged image INVALID (bad magic/length/crc)\r\n");
        /* 暂存区镜像非法: 保留现场供诊断, 状态清回 NORMAL 防止反复尝试 */
        ee_write_state(BOOT_STATE_NORMAL);
        ee_write_cnt(0U);
    }
    else
    {
        /* 打印新版本与长度/CRC 便于调试 */
        bl_uart_write("  version ");
        bl_uart_putc((char)('0' + (hdr.ver_major & 0x0FU)));
        bl_uart_putc('.');
        bl_uart_putc((char)('0' + (hdr.ver_minor & 0x0FU)));
        bl_uart_write("  len=");
        bl_uart_write_hex32(hdr.length);
        bl_uart_write("  crc=");
        bl_uart_write_hex32(hdr.crc32);
        bl_uart_write("\r\n");

        /* 必须先备份成功才允许烧写, 否则升级失败将无法回滚 */
        if (!backup_current_app())
        {
            bl_uart_write("[BL] backup FAILED, update aborted (old app kept)\r\n");
            ee_write_state(BOOT_STATE_NORMAL);
            ee_write_cnt(0U);
        }
        else if (flash_payload(W25Q_STAGING_ADDR, &hdr))
        {
            /* 烧写成功: 状态转 FLASHED, 清零失败计数, 记录新版本 */
            bl_uart_write("[BL] flash OK\r\n");
            ee_write_state(BOOT_STATE_FLASHED);
            ee_write_cnt(0U);
            ee_write_new_version(&hdr);
        }
        else
        {
            /* 烧写失败: 清状态并立即尝试从备份恢复旧 App */
            bl_uart_write("[BL] flash FAILED, restoring backup\r\n");
            ee_write_state(BOOT_STATE_NORMAL);
            ee_write_cnt(0U);
            if (!restore_from_backup())
            {
                bl_uart_write("[BL] RESTORE FAILED - manual recovery required\r\n");
            }
        }
    }
}

/* 回滚: 新 App 无法启动时, 从备份槽恢复旧 App */
static void do_rollback(void)
{
    bl_uart_write("[BL] app failed to confirm, rolling back\r\n");
    if (restore_from_backup())
    {
        ee_write_state(BOOT_STATE_NORMAL);
        ee_write_cnt(0U);
    }
    else
    {
        bl_uart_write("[BL] rollback FAILED - manual recovery required\r\n");
    }
}

/* ============================ 串口控制台 =================================
 * 仅在没有有效 App / App 主动请求 / 上电 300ms 内按 'b' 时进入。
 * 单字符命令, 每 200ms 轮询一次输入, 期间持续喂狗。
 * ========================================================================= */

/* 打印基本信息(状态/失败计数/App 是否有效) */
static void print_info(void)
{
    uint8_t state;
    uint8_t cnt;

    bl_uart_write("\r\n=== SCAN Bootloader v1.0 ===\r\n");
    bl_uart_write("App base: 0x08004000 size: 48KB\r\n");
    if (ee_read(&state, &cnt))
    {
        bl_uart_write("EE state: 0x");
        bl_uart_write_hex32(state);
        bl_uart_write(" attempts: ");
        bl_uart_putc((char)('0' + (cnt & 0x0FU)));
        bl_uart_write("\r\n");
    }
    else
    {
        bl_uart_write("EE state: unreadable\r\n");
    }
    bl_uart_write("App valid: ");
    bl_uart_write(bl_app_is_valid() ? "YES\r\n" : "NO\r\n");
}

/* 打印详细状态(暂存区/备份区是否有效) */
static void print_status(void)
{
    OtaImageHeader h;

    print_info();
    if (read_header(W25Q_STAGING_ADDR, &h))
    {
        bl_uart_write("Staging: magic=0x");
        bl_uart_write_hex32(h.magic);
        bl_uart_write(" len=0x");
        bl_uart_write_hex32(h.length);
        bl_uart_write(" valid=");
        bl_uart_write(validate_image(W25Q_STAGING_ADDR, OTA_MAGIC_STAGING, &h) ? "YES" : "NO");
        bl_uart_write("\r\n");
    }
    else
    {
        bl_uart_write("Staging: unreadable\r\n");
    }
    if (read_header(W25Q_BACKUP_ADDR, &h))
    {
        bl_uart_write("Backup: magic=0x");
        bl_uart_write_hex32(h.magic);
        bl_uart_write(" valid=");
        bl_uart_write(validate_image(W25Q_BACKUP_ADDR, OTA_MAGIC_BACKUP, &h) ? "YES" : "NO");
        bl_uart_write("\r\n");
    }
    else
    {
        bl_uart_write("Backup: unreadable\r\n");
    }
}

/* 打印帮助菜单 */
static void print_help(void)
{
    bl_uart_write("Commands:\r\n");
    bl_uart_write("  H help   V info    S status\r\n");
    bl_uart_write("  U jump app   F flash staged   B backup now\r\n");
    bl_uart_write("  E erase staging   X erase backup   R reset\r\n");
}

/* 控制台主循环: 永不返回(除非执行跳转/复位命令) */
static void console_run(void)
{
    char c;

    bl_uart_write("[BL] console mode (H=help)\r\n");
    for (;;)
    {
        bl_wdg_feed();                          /* 控制台等待期间持续喂狗 */
        if (bl_uart_getc(200U, &c))
        {
            switch (c)
            {
                case 'h': case 'H': case '?':
                    print_help();
                    break;
                case 'v': case 'V':
                    print_info();
                    break;
                case 's': case 'S':
                    print_status();
                    break;
                case 'u': case 'U':             /* 手动跳转 App */
                    if (bl_app_is_valid())
                    {
                        bl_jump_to_app();
                    }
                    else
                    {
                        bl_uart_write("no valid app\r\n");
                    }
                    break;
                case 'f': case 'F':             /* 强制用暂存区固件升级 */
                    do_update();
                    break;
                case 'b': case 'B':             /* 手动备份当前 App */
                    (void)backup_current_app();
                    break;
                case 'e': case 'E':             /* 擦除暂存区 */
                    bl_uart_write("[BL] erase staging\r\n");
                    (void)bl_w25q32_erase_range(W25Q_STAGING_ADDR, APP_MAX_SIZE + OTA_HEADER_SIZE);
                    bl_uart_write("  done\r\n");
                    break;
                case 'x': case 'X':             /* 擦除备份区 */
                    bl_uart_write("[BL] erase backup\r\n");
                    (void)bl_w25q32_erase_range(W25Q_BACKUP_ADDR, APP_MAX_SIZE + OTA_HEADER_SIZE);
                    bl_uart_write("  done\r\n");
                    break;
                case 'r': case 'R':             /* 软件复位 */
                    bl_uart_write("resetting...\r\n");
                    NVIC_SystemReset();
                    break;
                default:
                    break;
            }
        }
    }
}

/* ============================ 主函数 ===================================== */
int main(void)
{
    uint8_t state = BOOT_STATE_NORMAL;          /* 默认按正常启动处理       */
    uint8_t cnt = 0U;                           /* 启动失败计数             */
    uint8_t ee_ok;                              /* EEPROM 是否可读           */
    char c;

    /* ---- 1. 底层初始化 ---- */
    bl_system_init();                           /* HSE→PLL→72MHz           */
    bl_delay_init();                            /* SysTick 1ms 轮询时基     */
    bl_wdg_init();                              /* 启动独立看门狗           */
    bl_uart_init();                             /* USART1 115200            */
    bl_spi_init();                              /* SPI1 -> W25Q32           */
    bl_i2c_init();                              /* I2C2 -> W24C02           */
    bl_crc32_init();                            /* 生成 CRC 查找表          */
    bl_flash_init();                            /* 解锁内部 Flash           */

    bl_uart_write("\r\n[SCAN Bootloader v1.0] HSE-72MHz ready\r\n");
    bl_wdg_feed();

    /* ---- 2. 读取升级状态(读失败按 NORMAL 处理, 不阻塞启动) ---- */
    ee_ok = ee_read(&state, &cnt);
    if (!ee_ok)
    {
        bl_uart_write("[BL] warning: EEPROM unreadable, assuming normal boot\r\n");
        state = BOOT_STATE_NORMAL;
    }

    /* ---- 3. 上电 300ms 内按 'b' 强制进入控制台(方便接调试线恢复) ---- */
    if (BL_CONSOLE_ENABLE)
    {
        if (bl_uart_getc(300U, &c) && ((c == 'b') || (c == 'B')))
        {
            bl_uart_write("forced console\r\n");
            console_run();
        }
    }

    /* ---- 4. 状态机分支 ---- */
    if (state == BOOT_STATE_ENTER_BL)
    {
        /* App 主动请求进入 Bootloader: 清状态后进控制台 */
        ee_write_state(BOOT_STATE_NORMAL);
        ee_write_cnt(0U);
        bl_uart_write("[BL] app requested bootloader entry\r\n");
        console_run();
    }
    else if (state == BOOT_STATE_STAGED)
    {
        /* 新固件已暂存: 执行升级 */
        do_update();
    }
    else if (state == BOOT_STATE_FLASHED)
    {
        /* 新 App 已烧写但尚未确认: 计数并判断是否回滚 */
        cnt++;
        if (ee_ok)
        {
            ee_write_cnt(cnt);
        }
        if (cnt > MAX_BOOT_ATTEMPTS)
        {
            do_rollback();                      /* 连续多次未确认 → 回滚 */
        }
        else if (!bl_app_is_valid())
        {
            bl_uart_write("[BL] app invalid after flash, restoring backup\r\n");
            do_rollback();
        }
    }
    /* BOOT_STATE_NORMAL 及其它情况直接落到下面的"跳转" */

    /* ---- 5. 有有效 App 就跳转, 否则进控制台 ---- */
    if (bl_app_is_valid())
    {
        bl_jump_to_app();
    }

    bl_uart_write("[BL] no valid application found\r\n");
    console_run();

    return 0;
}
