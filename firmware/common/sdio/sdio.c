/**
 * @file sdio.c
 * @author Tilen Majerle, modified by Luke Oxley for PER usage (lcoxley@purdue.edu)
 * @brief Secure Digital Input Output Interface driver for STM32F4
 * @version 0.1
 * @date 2023-12-31
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "sdio.h"

#include "main.h"
#include "string.h"
//#define SD_LOG(...) log_msg(__VA_ARGS__)
#define SD_LOG(...)

static uint32_t CardType = SDIO_STD_CAPACITY_SD_CARD_V1_1;
static uint32_t CSD_Tab[4], CID_Tab[4], RCA = 0;
static uint8_t SDSTATUS_Tab[16];
__IO SD_Error TransferError = SD_OK;
__IO uint32_t TransferEnd = 0, DMAEndOfTransfer = 0;
SD_CardInfo SDCardInfo;
static uint32_t last_read_length = 0;

// Command response error checking prototypes
static SD_Error CmdError(void);
static SD_Error CmdResp1Error(uint8_t cmd);
static SD_Error CmdResp7Error(void);
static SD_Error CmdResp3Error(void);
static SD_Error CmdResp2Error(void);
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t* prca);
static SD_Error SDEnWideBus(FunctionalState NewState);
static SD_Error FindSCR(uint16_t rca, uint32_t* pscr);

/**
 * @brief Configures the SDIO peripheral
 *
 * @param cfg
 * @return true Success
 * @return false Fail
 */
bool PHAL_SDIO_init(void) {
    // GPIO Pins are to be initialized in main.c

    // Globals
    CardType = SDIO_STD_CAPACITY_SD_CARD_V1_1;
    memset(CSD_Tab, 0, sizeof(CSD_Tab));
    memset(CID_Tab, 0, sizeof(CID_Tab));
    memset(SDSTATUS_Tab, 0, sizeof(SDSTATUS_Tab));
    RCA              = 0;
    TransferError    = SD_OK;
    TransferEnd      = 0;
    DMAEndOfTransfer = 0;
    SDCardInfo       = (SD_CardInfo) {0};

    RCC->APB2ENR |= RCC_APB2ENR_SDIOEN;

    // SDIO IRQ priority must be lower than DMA IRQ (STM SD HAL said so)
    NVIC_SetPriority(SDIO_IRQn, 7);
    NVIC_EnableIRQ(SDIO_IRQn);
    NVIC_SetPriority(DMA2_Stream6_IRQn, 6);
    NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    if (SD_Init() == SD_OK) {
        return true;
    }

    return false;
}

/**
 * @brief  Initializes the SD Card and put it into StandBy State (Ready for data
 *         transfer).
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_Init(void) {
    __IO SD_Error errorstatus = SD_OK;
    
    SDIO->POWER |= SDIO_POWER_PWRCTRL; // Power on SDIO Peripheral
    SDIO->CLKCR = (SDIO_INIT_CLK_DIV & SDIO_CLKCR_CLKDIV_Msk) | SDIO_CLKCR_CLKEN; // Enable clock

    // Wait ~1 ms (Required power up waiting time before starting SD initialization sequence)
    // TODO: replace with vTaskDelay? Or some other form of delay
    for (uint32_t i = 0; i < 128000; ++i)
        ;

    /* Determine SD card operating voltage */
    errorstatus = SD_PowerON();

    if (errorstatus != SD_OK) {
        SD_LOG("SD_PowerON failed\r\n");
        return (errorstatus);
    }

    SD_LOG("SD_PowerON OK\r\n");

    errorstatus = SD_InitializeCards();

    if (errorstatus != SD_OK) {
        SD_LOG("SD_InitializeCards failed\r\n");
        /*!< CMD Response TimeOut (wait for CMDSENT flag) */
        return (errorstatus);
    }

    SD_LOG("SD_InitializeCards OK\r\n");

    // TODO: STM32 official SDIO/SDMMC HAL does not do this, maybe try without?
    SDIO->POWER |= SDIO_POWER_PWRCTRL; // Power on (clock the card)
    SDIO->CLKCR = (SDIO_TRANSFER_CLK_DIV & SDIO_CLKCR_CLKDIV) | SDIO_CLKCR_CLKEN;

    /*----------------- Read CSD/CID MSD registers ------------------*/
    errorstatus = SD_GetCardInfo(&SDCardInfo);

    if (errorstatus == SD_OK) {
        /*----------------- Select Card --------------------------------*/
        SD_LOG("SD_GetCardInfo OK\r\n");
        errorstatus = SD_SelectDeselect((uint32_t)(SDCardInfo.RCA << 16));
    } else {
        SD_LOG("SD_SelectDeselect failed\r\n");
    }

    if (errorstatus == SD_OK) {
        SD_LOG("SD_SelectDeselect OK\r\n");
#if FATFS_SDIO_4BIT == 1
        //4 bit mode
        errorstatus = SD_EnableWideBusOperation(SDIO_BusWide_4b);
#else
        //1 bit mode
        errorstatus = SD_EnableWideBusOperation(SDIO_BusWide_1b);
#endif
    } else {
        SD_LOG("SD_EnableWideBusOperation failed\r\n");
    }

    if (errorstatus == SD_OK) {
        SD_LOG("SD_EnableWideBusOperation OK\r\n");
    }

    return (errorstatus);
}

/**
 * @brief  Gets the cuurent sd card data transfer status.
 * @param  None
 * @retval SDTransferState: Data Transfer state.
 *   This value can be:
 *        - SD_TRANSFER_OK: No data transfer is acting
 *        - SD_TRANSFER_BUSY: Data transfer is acting
 */
SDTransferState SD_GetStatus(void) {
    SDCardState cardstate = SD_CARD_TRANSFER;

    cardstate = SD_GetState();

    if (cardstate == SD_CARD_TRANSFER) {
        return (SD_TRANSFER_OK);
    } else if (cardstate == SD_CARD_ERROR) {
        return (SD_TRANSFER_ERROR);
    } else {
        return (SD_TRANSFER_BUSY);
    }
}

/**
 * @brief  Returns the current card's state.
 * @param  None
 * @retval SDCardState: SD Card Error or SD Card Current State.
 */
SDCardState SD_GetState(void) {
    uint32_t resp1 = 0;

    if (SD_Detect() == SD_PRESENT) {
        if (SD_SendStatus(&resp1) != SD_OK) {
            return SD_CARD_ERROR;
        } else {
            return (SDCardState)((resp1 >> 9) & 0x0F);
        }
    }

    return SD_CARD_ERROR;
}

/**
 * @brief  Detect if SD card is correctly plugged in the memory slot.
 * @param  None
 * @retval Return if SD is detected or not
 */
uint8_t SD_Detect(void) {
    __IO uint8_t status = SD_PRESENT;

    /* Check status */
    if (PHAL_readGPIO(SD_CD_PORT, SD_CD_PIN)) {
        status = SD_NOT_PRESENT;
    }

    /* Return status */
    return status;
}

/**
 * @brief  Enquires cards about their operating voltage and configures
 *   clock controls.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_PowerON(void) {
    __IO SD_Error errorstatus = SD_OK;
    uint32_t SDType = SD_STD_CAPACITY;
    PHAL_SD_Cmd_t cmd;

    /*!< CMD0: GO_IDLE_STATE ---------------------------------------------------*/
    /*!< No CMD response required */
    cmd.idx = SD_CMD_GO_IDLE_STATE;
    cmd.res = SD_RESP_NONE;
    cmd.arg = 0;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdError();

    if (errorstatus != SD_OK) {
        /*!< CMD Response TimeOut (wait for CMDSENT flag) */
        return (errorstatus);
    }

    // Wait ~1 ms (If card reset by CMD0, not power on, takes time to reset)
    for (uint32_t i = 0; i < 128000; ++i)
        ;

    /*!< CMD8: SEND_IF_COND ----------------------------------------------------*/
    /*!< Send CMD8 to verify SD card interface operating condition */
    /*!< Argument: - [31:12]: Reserved (shall be set to '0')
	- [11:8]: Supply Voltage (VHS) 0x1 (Range: 2.7-3.6 V)
	- [7:0]: Check Pattern (recommended 0xAA) */
    /*!< CMD Response: R7 */
    cmd.idx = SDIO_SEND_IF_COND;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = SD_CHECK_PATTERN;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp7Error();

    if (errorstatus == SD_OK) {
        CardType = SDIO_STD_CAPACITY_SD_CARD_V2_0; /*!< SD Card 2.0 */
        SDType   = SD_HIGH_CAPACITY;
    } else {
        /*!< CMD55 */
        cmd.idx = SD_CMD_APP_CMD;
        cmd.res = SD_RESP_SHORT;
        cmd.arg = 0;
        PHAL_SDIO_SendCommand(&cmd);
        errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
    }

    /*!< CMD55 */
    cmd.idx = SD_CMD_APP_CMD;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = 0;
    PHAL_SDIO_SendCommand(&cmd);
    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

    /*!< If errorstatus is Command TimeOut, it is a MMC card */
    /*!< If errorstatus is SD_OK it is a SD card: SD card 2.0 (voltage range mismatch)
	or SD card 1.x */
    if (errorstatus == SD_OK) {
        uint32_t response = 0;
        uint32_t count = 0;
        uint32_t validvoltage = 0;
        /*!< SD CARD */
        /*!< Send ACMD41 SD_APP_OP_COND with Argument 0x80100000 */
        while ((!validvoltage) && (count < SD_MAX_VOLT_TRIAL)) {
            /*!< SEND CMD55 APP_CMD with RCA as 0 */
            cmd.idx = SD_CMD_APP_CMD;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = 0;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

            if (errorstatus != SD_OK) {
                return (errorstatus);
            }

            cmd.idx = SD_CMD_SD_APP_OP_COND;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = SD_VOLTAGE_WINDOW_SD | SDType;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp3Error();
            if (errorstatus != SD_OK) {
                return (errorstatus);
            }

            response     = SDIO->RESP1;
            validvoltage = (((response >> 31) == 1) ? 1 : 0);
            count++;
        }
        if (count >= SD_MAX_VOLT_TRIAL) {
            errorstatus = SD_INVALID_VOLTRANGE;
            return (errorstatus);
        }

        if ((response & SD_HIGH_CAPACITY) != 0) {
            CardType = SDIO_HIGH_CAPACITY_SD_CARD;
        }

    } /*!< else MMC Card */

    return (errorstatus);
}

/**
 * @brief  Intialises all cards or single card as the case may be Card(s) come
 *         into standby state.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_InitializeCards(void) {
    SD_Error errorstatus;
    uint16_t rca         = 0x01;
    PHAL_SD_Cmd_t cmd;

    // Check that SDIO peripheral is powered
    if ((SDIO->POWER & SDIO_POWER_PWRCTRL) == 0) {
        return SD_REQUEST_NOT_APPLICABLE;
    }

    if (SDIO_SECURE_DIGITAL_IO_CARD != CardType) {
        /*!< Send CMD2 ALL_SEND_CID */
        cmd.idx = SD_CMD_ALL_SEND_CID;
        cmd.res = SD_RESP_LONG;
        cmd.arg = 0;
        PHAL_SDIO_SendCommand(&cmd);

        errorstatus = CmdResp2Error();

        if (SD_OK != errorstatus) {
            return (errorstatus);
        }

        CID_Tab[0] = SDIO->RESP1;
        CID_Tab[1] = SDIO->RESP2;
        CID_Tab[2] = SDIO->RESP3;
        CID_Tab[3] = SDIO->RESP4;
    }

    if (
        (SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType)) {
        /*!< Send CMD3 SET_REL_ADDR with argument 0 */
        /*!< SD Card publishes its RCA. */
        cmd.idx = SD_CMD_SET_REL_ADDR;
        cmd.res = SD_RESP_SHORT;
        cmd.arg = 0;
        PHAL_SDIO_SendCommand(&cmd);

        errorstatus = CmdResp6Error(SD_CMD_SET_REL_ADDR, &rca);

        if (SD_OK != errorstatus) {
            return (errorstatus);
        }
    }

    if (SDIO_SECURE_DIGITAL_IO_CARD != CardType) {
        RCA = rca;

        /*!< Send CMD9 SEND_CSD with argument as card's RCA */
        cmd.idx = SD_CMD_SEND_CSD;
        cmd.res = SD_RESP_LONG;
        cmd.arg = (uint32_t)(rca << 16);
        PHAL_SDIO_SendCommand(&cmd);

        errorstatus = CmdResp2Error();

        if (SD_OK != errorstatus) {
            return (errorstatus);
        }

        CID_Tab[0] = SDIO->RESP1;
        CID_Tab[1] = SDIO->RESP2;
        CID_Tab[2] = SDIO->RESP3;
        CID_Tab[3] = SDIO->RESP4;
    }

    return SD_OK;
}

/**
 * @brief  Returns information about specific card.
 * @param  cardinfo: pointer to a SD_CardInfo structure that contains all SD card
 *         information.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_GetCardInfo(SD_CardInfo* cardinfo) {
    SD_Error errorstatus = SD_OK;
    uint8_t tmp          = 0;

    cardinfo->CardType = (uint8_t)CardType;
    cardinfo->RCA      = (uint16_t)RCA;

    /*!< Byte 0 */
    tmp                             = (uint8_t)((CSD_Tab[0] & 0xFF000000) >> 24);
    cardinfo->SD_csd.CSDStruct      = (tmp & 0xC0) >> 6;
    cardinfo->SD_csd.SysSpecVersion = (tmp & 0x3C) >> 2;
    cardinfo->SD_csd.Reserved1      = tmp & 0x03;

    /*!< Byte 1 */
    tmp                   = (uint8_t)((CSD_Tab[0] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.TAAC = tmp;

    /*!< Byte 2 */
    tmp                   = (uint8_t)((CSD_Tab[0] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.NSAC = tmp;

    /*!< Byte 3 */
    tmp                            = (uint8_t)(CSD_Tab[0] & 0x000000FF);
    cardinfo->SD_csd.MaxBusClkFrec = tmp;

    /*!< Byte 4 */
    tmp                              = (uint8_t)((CSD_Tab[1] & 0xFF000000) >> 24);
    cardinfo->SD_csd.CardComdClasses = tmp << 4;

    /*!< Byte 5 */
    tmp = (uint8_t)((CSD_Tab[1] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.CardComdClasses |= (tmp & 0xF0) >> 4;
    cardinfo->SD_csd.RdBlockLen = tmp & 0x0F;

    /*!< Byte 6 */
    tmp                              = (uint8_t)((CSD_Tab[1] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.PartBlockRead   = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.WrBlockMisalign = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.RdBlockMisalign = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.DSRImpl         = (tmp & 0x10) >> 4;
    cardinfo->SD_csd.Reserved2       = 0; /*!< Reserved */

    if ((CardType == SDIO_STD_CAPACITY_SD_CARD_V1_1) || (CardType == SDIO_STD_CAPACITY_SD_CARD_V2_0)) {
        cardinfo->SD_csd.DeviceSize = (tmp & 0x03) << 10;

        /*!< Byte 7 */
        tmp = (uint8_t)(CSD_Tab[1] & 0x000000FF);
        cardinfo->SD_csd.DeviceSize |= (tmp) << 2;

        /*!< Byte 8 */
        tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);
        cardinfo->SD_csd.DeviceSize |= (tmp & 0xC0) >> 6;

        cardinfo->SD_csd.MaxRdCurrentVDDMin = (tmp & 0x38) >> 3;
        cardinfo->SD_csd.MaxRdCurrentVDDMax = (tmp & 0x07);

        /*!< Byte 9 */
        tmp                                 = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);
        cardinfo->SD_csd.MaxWrCurrentVDDMin = (tmp & 0xE0) >> 5;
        cardinfo->SD_csd.MaxWrCurrentVDDMax = (tmp & 0x1C) >> 2;
        cardinfo->SD_csd.DeviceSizeMul      = (tmp & 0x03) << 1;
        /*!< Byte 10 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);
        cardinfo->SD_csd.DeviceSizeMul |= (tmp & 0x80) >> 7;

        cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1);
        cardinfo->CardCapacity *= (1 << (cardinfo->SD_csd.DeviceSizeMul + 2));
        cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);
        cardinfo->CardCapacity *= cardinfo->CardBlockSize;
    } else if (CardType == SDIO_HIGH_CAPACITY_SD_CARD) {
        /*!< Byte 7 */
        tmp                         = (uint8_t)(CSD_Tab[1] & 0x000000FF);
        cardinfo->SD_csd.DeviceSize = (tmp & 0x3F) << 16;

        /*!< Byte 8 */
        tmp = (uint8_t)((CSD_Tab[2] & 0xFF000000) >> 24);

        cardinfo->SD_csd.DeviceSize |= (tmp << 8);

        /*!< Byte 9 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x00FF0000) >> 16);

        cardinfo->SD_csd.DeviceSize |= (tmp);

        /*!< Byte 10 */
        tmp = (uint8_t)((CSD_Tab[2] & 0x0000FF00) >> 8);

        cardinfo->CardCapacity  = ((uint64_t)cardinfo->SD_csd.DeviceSize + 1) * 512 * 1024;
        cardinfo->CardBlockSize = 512;
    }

    cardinfo->SD_csd.EraseGrSize = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.EraseGrMul  = (tmp & 0x3F) << 1;

    /*!< Byte 11 */
    tmp = (uint8_t)(CSD_Tab[2] & 0x000000FF);
    cardinfo->SD_csd.EraseGrMul |= (tmp & 0x80) >> 7;
    cardinfo->SD_csd.WrProtectGrSize = (tmp & 0x7F);

    /*!< Byte 12 */
    tmp                                = (uint8_t)((CSD_Tab[3] & 0xFF000000) >> 24);
    cardinfo->SD_csd.WrProtectGrEnable = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.ManDeflECC        = (tmp & 0x60) >> 5;
    cardinfo->SD_csd.WrSpeedFact       = (tmp & 0x1C) >> 2;
    cardinfo->SD_csd.MaxWrBlockLen     = (tmp & 0x03) << 2;

    /*!< Byte 13 */
    tmp = (uint8_t)((CSD_Tab[3] & 0x00FF0000) >> 16);
    cardinfo->SD_csd.MaxWrBlockLen |= (tmp & 0xC0) >> 6;
    cardinfo->SD_csd.WriteBlockPaPartial = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.Reserved3           = 0;
    cardinfo->SD_csd.ContentProtectAppli = (tmp & 0x01);

    /*!< Byte 14 */
    tmp                               = (uint8_t)((CSD_Tab[3] & 0x0000FF00) >> 8);
    cardinfo->SD_csd.FileFormatGrouop = (tmp & 0x80) >> 7;
    cardinfo->SD_csd.CopyFlag         = (tmp & 0x40) >> 6;
    cardinfo->SD_csd.PermWrProtect    = (tmp & 0x20) >> 5;
    cardinfo->SD_csd.TempWrProtect    = (tmp & 0x10) >> 4;
    cardinfo->SD_csd.FileFormat       = (tmp & 0x0C) >> 2;
    cardinfo->SD_csd.ECC              = (tmp & 0x03);

    /*!< Byte 15 */
    tmp                        = (uint8_t)(CSD_Tab[3] & 0x000000FF);
    cardinfo->SD_csd.CSD_CRC   = (tmp & 0xFE) >> 1;
    cardinfo->SD_csd.Reserved4 = 1;

    /*!< Byte 0 */
    tmp                             = (uint8_t)((CID_Tab[0] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ManufacturerID = tmp;

    /*!< Byte 1 */
    tmp                          = (uint8_t)((CID_Tab[0] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.OEM_AppliID = tmp << 8;

    /*!< Byte 2 */
    tmp = (uint8_t)((CID_Tab[0] & 0x000000FF00) >> 8);
    cardinfo->SD_cid.OEM_AppliID |= tmp;

    /*!< Byte 3 */
    tmp                        = (uint8_t)(CID_Tab[0] & 0x000000FF);
    cardinfo->SD_cid.ProdName1 = tmp << 24;

    /*!< Byte 4 */
    tmp = (uint8_t)((CID_Tab[1] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdName1 |= tmp << 16;

    /*!< Byte 5 */
    tmp = (uint8_t)((CID_Tab[1] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.ProdName1 |= tmp << 8;

    /*!< Byte 6 */
    tmp = (uint8_t)((CID_Tab[1] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ProdName1 |= tmp;

    /*!< Byte 7 */
    tmp                        = (uint8_t)(CID_Tab[1] & 0x000000FF);
    cardinfo->SD_cid.ProdName2 = tmp;

    /*!< Byte 8 */
    tmp                      = (uint8_t)((CID_Tab[2] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdRev = tmp;

    /*!< Byte 9 */
    tmp                     = (uint8_t)((CID_Tab[2] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.ProdSN = tmp << 24;

    /*!< Byte 10 */
    tmp = (uint8_t)((CID_Tab[2] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ProdSN |= tmp << 16;

    /*!< Byte 11 */
    tmp = (uint8_t)(CID_Tab[2] & 0x000000FF);
    cardinfo->SD_cid.ProdSN |= tmp << 8;

    /*!< Byte 12 */
    tmp = (uint8_t)((CID_Tab[3] & 0xFF000000) >> 24);
    cardinfo->SD_cid.ProdSN |= tmp;

    /*!< Byte 13 */
    tmp = (uint8_t)((CID_Tab[3] & 0x00FF0000) >> 16);
    cardinfo->SD_cid.Reserved1 |= (tmp & 0xF0) >> 4;
    cardinfo->SD_cid.ManufactDate = (tmp & 0x0F) << 8;

    /*!< Byte 14 */
    tmp = (uint8_t)((CID_Tab[3] & 0x0000FF00) >> 8);
    cardinfo->SD_cid.ManufactDate |= tmp;

    /*!< Byte 15 */
    tmp                        = (uint8_t)(CID_Tab[3] & 0x000000FF);
    cardinfo->SD_cid.CID_CRC   = (tmp & 0xFE) >> 1;
    cardinfo->SD_cid.Reserved2 = 1;

    return (errorstatus);
}

/**
 * @brief  Enables wide bus opeartion for the requeseted card if supported by
 *         card.
 * @param  WideMode: Specifies the SD card wide bus mode.
 *   This parameter can be one of the following values:
 *     @arg SDIO_BusWide_8b: 8-bit data transfer (Only for MMC)
 *     @arg SDIO_BusWide_4b: 4-bit data transfer
 *     @arg SDIO_BusWide_1b: 1-bit data transfer
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_EnableWideBusOperation(uint32_t WideMode) {
    SD_Error errorstatus = SD_OK;

    /*!< MMC Card doesn't support this feature */
    if (SDIO_MULTIMEDIA_CARD == CardType) {
        errorstatus = SD_UNSUPPORTED_FEATURE;
        return (errorstatus);
    } else if ((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType)) {
        if (SDIO_BusWide_8b == WideMode) {
            errorstatus = SD_UNSUPPORTED_FEATURE;
            return (errorstatus);
        } else if (SDIO_BusWide_4b == WideMode) {
            errorstatus = SDEnWideBus(ENABLE);

            if (SD_OK == errorstatus) {
                /*!< Configure the SDIO peripheral */
                // SDIO->CLKCR &= ~(0x7FFF); // clear
                // SDIO->CLKCR |= (SDIO_TRANSFER_CLK_DIV) & SDIO_CLKCR_CLKDIV_Msk | SDIO_CLKCR_WIDBUS_0;
                SDIO->POWER |= SDIO_POWER_PWRCTRL; // Power on (clock the card)
                // SDIO->CLKCR |= SDIO_CLKCR_CLKEN;   // Enable clock
                SDIO->CLKCR = (SDIO_TRANSFER_CLK_DIV & SDIO_CLKCR_CLKDIV_Msk) | SDIO_CLKCR_WIDBUS_0 | SDIO_CLKCR_CLKEN;
            }
        } else {
            errorstatus = SDEnWideBus(DISABLE);

            if (SD_OK == errorstatus) {
                /*!< Configure the SDIO peripheral */
                SDIO->CLKCR &= ~(0x7FFF); // clear
                SDIO->CLKCR |= (SDIO_TRANSFER_CLK_DIV)&SDIO_CLKCR_CLKDIV_Msk;
                SDIO->POWER |= SDIO_POWER_PWRCTRL; // Power on (clock the card)
                SDIO->CLKCR |= SDIO_CLKCR_CLKEN; // Enable clock
            }
        }
    }

    return (errorstatus);
}

/**
 * @brief  Selects od Deselects the corresponding card.
 * @param  addr: Address of the Card to be selected.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_SelectDeselect(uint64_t addr) {
    SD_Error errorstatus = SD_OK;
    PHAL_SD_Cmd_t cmd;

    /*!< Send CMD7 SDIO_SEL_DESEL_CARD */
    cmd.idx = SD_CMD_SEL_DESEL_CARD;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)addr;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SEL_DESEL_CARD);

    return (errorstatus);
}

/**
 * @brief Send a command
 *
 * @param cmd
 */
void PHAL_SDIO_SendCommand(const PHAL_SD_Cmd_t* cmd) {
    SDIO->ARG = cmd->arg;
    SDIO->CMD = ((cmd->idx & SDIO_CMD_CMDINDEX_Msk)) | ((cmd->res << SDIO_CMD_WAITRESP_Pos) & SDIO_CMD_WAITRESP_Msk) | SDIO_CMD_CPSMEN;
}

/**
 * @brief  Allows to read blocks from a specified address  in a card.  The Data
 *         transfer can be managed by DMA mode or Polling mode.
 * @note   This operation should be followed by two functions to check if the
 *         DMA Controller and SD Card status.
 *          - SD_ReadWaitOperation(): this function insure that the DMA
 *            controller has finished all data transfer.
 *          - SD_GetStatus(): to check that the SD Card has finished the
 *            data transfer and it is ready for data.
 * @param  readbuff: pointer to the buffer that will contain the received data.
 * @param  ReadAddr: Address from where data are to be read.
 * @param  BlockSize: the SD card Data block size. The Block size should be 512.
 * @param  NumberOfBlocks: number of blocks to be read.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_ReadMultiBlocks(uint8_t* readbuff, uint64_t ReadAddr, uint16_t BlockSize, uint32_t NumberOfBlocks) {
    SD_Error errorstatus = SD_OK;
    TransferError        = SD_OK;
    TransferEnd          = 0;
    last_read_length     = NumberOfBlocks;

    SDIO->DCTRL = 0x0;

    SDIO->MASK |= (SDIO_MASK_DCRCFAILIE | SDIO_MASK_DTIMEOUTIE | SDIO_MASK_DATAENDIE | SDIO_MASK_RXOVERRIE | SDIO_MASK_STBITERRIE);
    //if (NumberOfBlocks == 1) SDIO->MASK |= SDIO_MASK_DBCKENDIE;
    SD_LowLevel_DMA_RxConfig((uint32_t*)readbuff, (NumberOfBlocks * BlockSize));
    // TODO: check that hardware flow control set in CLKCR...
    // Hardware flow control for STM32F407xx boards said to cause glitches on SDIOCLK output clock :(
    // Hopefully that is not important/maybe doesn't apply to this version of the board?

    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD) {
        BlockSize = 512;
        ReadAddr /= 512;
    }

    /*!< Set Block Size for Card */
    PHAL_SD_Cmd_t cmd;
    cmd.idx = SD_CMD_SET_BLOCKLEN;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)BlockSize;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (SD_OK != errorstatus) {
        return (errorstatus);
    }

    SDIO->DTIMER = SD_DATATIMEOUT; // timeout
    SDIO->DLEN &= ~SDIO_DLEN_DATALENGTH; // length
    SDIO->DLEN |= NumberOfBlocks * BlockSize;
    SDIO->DCTRL &= ~(0xFFF); // start read
    SDIO->DCTRL |= SDIO_DATABLOCKSIZE | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;

    // if (NumberOfBlocks > 1)
    // {
    /*!< Send CMD18 READ_MULT_BLOCK with argument data address */
    cmd.idx = SD_CMD_READ_MULT_BLOCK;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)ReadAddr;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_READ_MULT_BLOCK);
    // }
    // else
    // {
    // 	/*!< Send CMD17 READ_SINGLE_BLOCK with argument data address */
    // 	cmd.idx = SD_CMD_READ_SINGLE_BLOCK;
    // 	cmd.res = SD_RESP_SHORT;
    // 	cmd.arg = (uint32_t) ReadAddr;
    // 	PHAL_SDIO_SendCommand(&cmd);

    // 	errorstatus = CmdResp1Error (SD_CMD_READ_SINGLE_BLOCK );
    // }

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    return (errorstatus);
}

/**
 * @brief  This function waits until the SDIO DMA data transfer is finished.
 *         This function should be called after SDIO_ReadMultiBlocks() function
 *         to insure that all data sent by the card are already transferred by
 *         the DMA controller.
 * @param  None.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_WaitReadOperation(void) {
    SD_Error errorstatus = SD_OK;
    volatile uint32_t timeout;

    timeout = SD_DATATIMEOUT;

    while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0)) {
        timeout--;
    }

    DMAEndOfTransfer = 0x00;

    timeout = SD_DATATIMEOUT;

    while (((SDIO->STA & SDIO_STA_RXACT)) && (timeout > 0)) {
        timeout--;
    }

    errorstatus = SD_StopTransfer(); // CMD12 Required for multiblock read/writes 

    if ((timeout == 0) && (errorstatus == SD_OK)) {
        errorstatus = SD_DATA_TIMEOUT;
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    if (TransferError != SD_OK) {
        return (TransferError);
    }

    return (errorstatus);
}

/**
 * @brief  Allows to write blocks starting from a specified address in a card.
 *         The Data transfer can be managed by DMA mode only.
 * @note   This operation should be followed by two functions to check if the
 *         DMA Controller and SD Card status.
 *          - SD_ReadWaitOperation(): this function insure that the DMA
 *            controller has finished all data transfer.
 *          - SD_GetStatus(): to check that the SD Card has finished the
 *            data transfer and it is ready for data.
 * @param  WriteAddr: Address from where data are to be read.
 * @param  writebuff: pointer to the buffer that contain the data to be transferred.
 * @param  BlockSize: the SD card Data block size. The Block size should be 512.
 * @param  NumberOfBlocks: number of blocks to be written.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_WriteMultiBlocks(uint8_t* writebuff, uint64_t WriteAddr, uint16_t BlockSize, uint32_t NumberOfBlocks) {
    SD_Error errorstatus = SD_OK;
    PHAL_SD_Cmd_t cmd;

    TransferError = SD_OK;
    TransferEnd   = 0;

    // Initialize data control register
    SDIO->DCTRL   = 0x0;
    SDIO->MASK |= (SDIO_MASK_DCRCFAILIE | SDIO_MASK_DTIMEOUTIE | SDIO_MASK_DATAENDIE | SDIO_MASK_TXUNDERRIE | SDIO_MASK_STBITERRIE);
    SD_LowLevel_DMA_TxConfig((uint32_t*)writebuff, (NumberOfBlocks * BlockSize));

    if (CardType == SDIO_HIGH_CAPACITY_SD_CARD) {
        BlockSize = 512;
        WriteAddr /= 512;
    }

    /* Set Block Size for Card */
    cmd.idx = SD_CMD_SET_BLOCKLEN;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)BlockSize;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (SD_OK != errorstatus) {
        return (errorstatus);
    }

    /*!< To improve performance */
    cmd.idx = SD_CMD_APP_CMD;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)(RCA << 16);
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    /*!< To improve performance */
    cmd.idx = SD_CMD_SET_BLOCK_COUNT;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)NumberOfBlocks;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    /*!< Send CMD25 WRITE_MULT_BLOCK with argument data address */
    cmd.idx = SD_CMD_WRITE_MULT_BLOCK;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)WriteAddr;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);

    if (SD_OK != errorstatus) {
        return (errorstatus);
    }

    SDIO->DTIMER = SD_DATATIMEOUT; // timeout
    SDIO->DLEN &= ~SDIO_DLEN_DATALENGTH; // length
    SDIO->DLEN |= NumberOfBlocks * BlockSize;
    SDIO->DCTRL &= ~(0xFFF); // clear DCTRL register 
    SDIO->DCTRL |= SDIO_DATABLOCKSIZE | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;

    return (errorstatus);
}

/**
 * @brief  This function waits until the SDIO DMA data transfer is finished.
 *         This function should be called after SDIO_WriteBlock() and
 *         SDIO_WriteMultiBlocks() function to insure that all data sent by the
 *         card are already transferred by the DMA controller.
 * @param  None.
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_WaitWriteOperation(void) {
    SD_Error errorstatus = SD_OK;
    uint32_t timeout;

    timeout = SD_DATATIMEOUT;

    while ((DMAEndOfTransfer == 0x00) && (TransferEnd == 0) && (TransferError == SD_OK) && (timeout > 0)) {
        timeout--;
    }

    DMAEndOfTransfer = 0x00;

    timeout = SD_DATATIMEOUT;

    while (((SDIO->STA & SDIO_STA_TXACT)) && (timeout > 0)) {
        timeout--;
    }

    errorstatus   = SD_StopTransfer();

    if ((timeout == 0) && (errorstatus == SD_OK)) {
        errorstatus = SD_DATA_TIMEOUT;
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    if (TransferError != SD_OK) {
        return (TransferError);
    } else {
        return (errorstatus);
    }
}

/**
 * @brief  Aborts an ongoing data transfer.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_StopTransfer(void) {
    SD_Error errorstatus = SD_OK;
    PHAL_SD_Cmd_t cmd;

    /*!< Send CMD12 STOP_TRANSMISSION  */
    cmd.idx = SD_CMD_STOP_TRANSMISSION;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = 0;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);

    // Try again
    if (errorstatus == SD_CMD_RSP_TIMEOUT) {
        PHAL_SDIO_SendCommand(&cmd);
        errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
    }

    return (errorstatus);
}

/**
 * @brief  Returns the current card's status.
 * @param  pcardstatus: pointer to the buffer that will contain the SD card
 *         status (Card Status register).
 * @retval SD_Error: SD Card Error code.
 */
SD_Error SD_SendStatus(uint32_t* pcardstatus) {
    SD_Error errorstatus = SD_OK;
    PHAL_SD_Cmd_t cmd;

    if (pcardstatus == 0) {
        errorstatus = SD_INVALID_PARAMETER;
        return (errorstatus);
    }

    cmd.idx = SD_CMD_SEND_STATUS;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)RCA << 16;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SEND_STATUS);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    *pcardstatus = SDIO->RESP1;

    return (errorstatus);
}

//---------------------------------------------------------
// Command Response Error Checking
//---------------------------------------------------------

/**
 * @brief  Checks for error conditions for CMD0.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdError(void) {
    SD_Error errorstatus = SD_OK;
    uint32_t timeout;
    timeout = SDIO_CMD0TIMEOUT; /*!< 10000 */

    while ((timeout > 0) && (SDIO->STA & SDIO_STA_CMDSENT)) {
        timeout--;
    }

    if (timeout == 0) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        return (errorstatus);
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    return (errorstatus);
}

/**
 * @brief  Checks for error conditions for R7 response.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdResp7Error(void) {
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t timeout = SDIO_CMD0TIMEOUT;

    status = SDIO->STA;

    while (!(status & (SDIO_STA_CCRCFAIL | SDIO_STA_CMDREND | SDIO_STA_CTIMEOUT)) && (timeout > 0)) {
        timeout--;
        status = SDIO->STA;
    }

    if ((timeout == 0) || (status & SDIO_STA_CTIMEOUT)) {
        /*!< Card is not V2.0 complient or card does not support the set voltage range */
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO->ICR   = (SDIO_ICR_CTIMEOUTC);
        return (errorstatus);
    }

    if (status & SDIO_STA_CMDREND) {
        /*!< Card is SD V2.0 compliant */
        errorstatus = SD_OK;
        SDIO->ICR   = (SDIO_ICR_CMDRENDC);
        return (errorstatus);
    }
    return (errorstatus);
}

/**
 * @brief  Checks for error conditions for R1 response.
 * @param  cmd: The sent command index.
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdResp1Error(uint8_t cmd) {
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t response_r1;

    status = SDIO->STA;

    while (!(status & (SDIO_STA_CCRCFAIL | SDIO_STA_CMDREND | SDIO_STA_CTIMEOUT))) {
        status = SDIO->STA;
    }

    if (status & SDIO_STA_CTIMEOUT) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO->ICR   = (SDIO_ICR_CTIMEOUTC);
        SD_LOG("cmd %d timed out with status 0x%x\n", cmd, status);
        return (errorstatus);
    } else if (status & SDIO_STA_CCRCFAIL) {
        errorstatus = SD_CMD_CRC_FAIL;
        SDIO->ICR   = (SDIO_ICR_CCRCFAILC);
        return (errorstatus);
    }

    /*!< Check response received is of desired command */
    if ((SDIO->RESPCMD & SDIO_RESPCMD_RESPCMD) != cmd) {
        errorstatus = SD_ILLEGAL_CMD;
        return (errorstatus);
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    /*!< We have received response, retrieve it for analysis  */
    response_r1 = SDIO->RESP1;

    if ((response_r1 & SD_OCR_ERRORBITS) == SD_ALLZERO) {
        return (errorstatus);
    }

    if (response_r1 & SD_OCR_ADDR_OUT_OF_RANGE) {
        return (SD_ADDR_OUT_OF_RANGE);
    }

    if (response_r1 & SD_OCR_ADDR_MISALIGNED) {
        return (SD_ADDR_MISALIGNED);
    }

    if (response_r1 & SD_OCR_BLOCK_LEN_ERR) {
        return (SD_BLOCK_LEN_ERR);
    }

    if (response_r1 & SD_OCR_ERASE_SEQ_ERR) {
        return (SD_ERASE_SEQ_ERR);
    }

    if (response_r1 & SD_OCR_BAD_ERASE_PARAM) {
        return (SD_BAD_ERASE_PARAM);
    }

    if (response_r1 & SD_OCR_WRITE_PROT_VIOLATION) {
        return (SD_WRITE_PROT_VIOLATION);
    }

    if (response_r1 & SD_OCR_LOCK_UNLOCK_FAILED) {
        return (SD_LOCK_UNLOCK_FAILED);
    }

    if (response_r1 & SD_OCR_COM_CRC_FAILED) {
        return (SD_COM_CRC_FAILED);
    }

    if (response_r1 & SD_OCR_ILLEGAL_CMD) {
        return (SD_ILLEGAL_CMD);
    }

    if (response_r1 & SD_OCR_CARD_ECC_FAILED) {
        return (SD_CARD_ECC_FAILED);
    }

    if (response_r1 & SD_OCR_CC_ERROR) {
        return (SD_CC_ERROR);
    }

    if (response_r1 & SD_OCR_GENERAL_UNKNOWN_ERROR) {
        return (SD_GENERAL_UNKNOWN_ERROR);
    }

    if (response_r1 & SD_OCR_STREAM_READ_UNDERRUN) {
        return (SD_STREAM_READ_UNDERRUN);
    }

    if (response_r1 & SD_OCR_STREAM_WRITE_OVERRUN) {
        return (SD_STREAM_WRITE_OVERRUN);
    }

    if (response_r1 & SD_OCR_CID_CSD_OVERWRIETE) {
        return (SD_CID_CSD_OVERWRITE);
    }

    if (response_r1 & SD_OCR_WP_ERASE_SKIP) {
        return (SD_WP_ERASE_SKIP);
    }

    if (response_r1 & SD_OCR_CARD_ECC_DISABLED) {
        return (SD_CARD_ECC_DISABLED);
    }

    if (response_r1 & SD_OCR_ERASE_RESET) {
        return (SD_ERASE_RESET);
    }

    if (response_r1 & SD_OCR_AKE_SEQ_ERROR) {
        return (SD_AKE_SEQ_ERROR);
    }
    return (errorstatus);
}

/**
 * @brief  Checks for error conditions for R3 (OCR) response.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdResp3Error(void) {
    SD_Error errorstatus = SD_OK;
    uint32_t status;

    status = SDIO->STA;

    while (!(status & (SDIO_STA_CCRCFAIL | SDIO_STA_CMDREND | SDIO_STA_CTIMEOUT))) {
        status = SDIO->STA;
    }

    if (status & SDIO_STA_CTIMEOUT) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO->ICR   = (SDIO_ICR_CTIMEOUTC);
        return (errorstatus);
    }
    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);
    return (errorstatus);
}

/**
 * @brief  Checks for error conditions for R2 (CID or CSD) response.
 * @param  None
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdResp2Error(void) {
    SD_Error errorstatus = SD_OK;
    uint32_t status;

    status = SDIO->STA;

    while (!(status & (SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND))) {
        status = SDIO->STA;
    }

    if (status & SDIO_STA_CTIMEOUT) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO->ICR   = (SDIO_ICR_CTIMEOUTC);
        return (errorstatus);
    } else if (status & SDIO_STA_CCRCFAIL) {
        errorstatus = SD_CMD_CRC_FAIL;
        SDIO->ICR   = (SDIO_ICR_CCRCFAILC);
        return (errorstatus);
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    return (errorstatus);
}

/**
 * @brief  Checks for error conditions for R6 (RCA) response.
 * @param  cmd: The sent command index.
 * @param  prca: pointer to the variable that will contain the SD card relative
 *         address RCA.
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error CmdResp6Error(uint8_t cmd, uint16_t* prca) {
    SD_Error errorstatus = SD_OK;
    uint32_t status;
    uint32_t response_r1;

    status = SDIO->STA;

    while (!(status & (SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND))) {
        status = SDIO->STA;
    }

    if (status & SDIO_STA_CTIMEOUT) {
        errorstatus = SD_CMD_RSP_TIMEOUT;
        SDIO->ICR   = (SDIO_ICR_CTIMEOUTC);
        return (errorstatus);
    } else if (status & SDIO_STA_CCRCFAIL) {
        errorstatus = SD_CMD_CRC_FAIL;
        SDIO->ICR   = (SDIO_ICR_CCRCFAILC);
        return (errorstatus);
    }

    /*!< Check response received is of desired command */
    if ((SDIO->RESPCMD & SDIO_RESPCMD_RESPCMD) != cmd) {
        errorstatus = SD_ILLEGAL_CMD;
        return (errorstatus);
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    /*!< We have received response, retrieve it.  */
    response_r1 = SDIO->RESP1;

    if (SD_ALLZERO == (response_r1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED))) {
        *prca = (uint16_t)(response_r1 >> 16);
        return (errorstatus);
    }

    if (response_r1 & SD_R6_GENERAL_UNKNOWN_ERROR) {
        return (SD_GENERAL_UNKNOWN_ERROR);
    }

    if (response_r1 & SD_R6_ILLEGAL_CMD) {
        return (SD_ILLEGAL_CMD);
    }

    if (response_r1 & SD_R6_COM_CRC_FAILED) {
        return (SD_COM_CRC_FAILED);
    }

    return (errorstatus);
}

/**
 * @brief  Enables or disables the SDIO wide bus mode.
 * @param  NewState: new state of the SDIO wide bus mode.
 *   This parameter can be: ENABLE or DISABLE.
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error SDEnWideBus(FunctionalState NewState) {
    SD_Error errorstatus = SD_OK;
    PHAL_SD_Cmd_t cmd;

    uint32_t scr[2] = {0, 0};

    if (SDIO->RESP1 & SD_CARD_LOCKED) {
        errorstatus = SD_LOCK_UNLOCK_FAILED;
        return (errorstatus);
    }

    /*!< Get SCR Register */
    errorstatus = FindSCR(RCA, scr);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    /*!< If wide bus operation to be enabled */
    if (NewState == ENABLE) {
        /*!< If requested card supports wide bus operation */
        if ((scr[1] & SD_WIDE_BUS_SUPPORT) != SD_ALLZERO) {
            /*!< Send CMD55 APP_CMD with argument as card's RCA.*/
            cmd.idx = SD_CMD_APP_CMD;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = (uint32_t)RCA << 16;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

            if (errorstatus != SD_OK) {
                return (errorstatus);
            }

            /*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
            cmd.idx = SD_CMD_APP_SD_SET_BUSWIDTH;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = 0x2;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

            if (errorstatus != SD_OK) {
                return (errorstatus);
            }
            return (errorstatus);
        } else {
            errorstatus = SD_REQUEST_NOT_APPLICABLE;
            return (errorstatus);
        }
    } else { /*!< If wide bus operation to be disabled */
        /*!< If requested card supports 1 bit mode operation */
        if ((scr[1] & SD_SINGLE_BUS_SUPPORT) != SD_ALLZERO) {
            /*!< Send CMD55 APP_CMD with argument as card's RCA.*/
            cmd.idx = SD_CMD_APP_CMD;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = (uint32_t)RCA << 16;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

            if (errorstatus != SD_OK) {
                return (errorstatus);
            }

            /*!< Send ACMD6 APP_CMD with argument as 2 for wide bus mode */
            cmd.idx = SD_CMD_APP_SD_SET_BUSWIDTH;
            cmd.res = SD_RESP_SHORT;
            cmd.arg = 0x00;
            PHAL_SDIO_SendCommand(&cmd);

            errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);

            if (errorstatus != SD_OK) {
                return (errorstatus);
            }

            return (errorstatus);
        } else {
            errorstatus = SD_REQUEST_NOT_APPLICABLE;
            return (errorstatus);
        }
    }
}

/**
 * @brief  Find the SD card SCR register value.
 * @param  rca: selected card address.
 * @param  pscr: pointer to the buffer that will contain the SCR value.
 * @retval SD_Error: SD Card Error code.
 */
static SD_Error FindSCR(uint16_t rca, uint32_t* pscr) {
    uint32_t index       = 0;
    SD_Error errorstatus = SD_OK;
    uint32_t tempscr[2]  = {0, 0};
    PHAL_SD_Cmd_t cmd;

    /*!< Set Block Size To 8 Bytes */
    /*!< Send CMD55 APP_CMD with argument as card's RCA */
    cmd.idx = SD_CMD_SET_BLOCKLEN;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)8;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    /*!< Send CMD55 APP_CMD with argument as card's RCA */
    cmd.idx = SD_CMD_APP_CMD;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = (uint32_t)RCA << 16;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_APP_CMD);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    SDIO->DTIMER = SD_DATATIMEOUT;
    SDIO->DLEN &= ~SDIO_DLEN_DATALENGTH;
    SDIO->DLEN |= 8;
    SDIO->DCTRL &= ~(0xFFF);
    SDIO->DCTRL |= (SDIO_DCTRL_DBLOCKSIZE_1 | SDIO_DCTRL_DBLOCKSIZE_0) | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DTEN;

    /*!< Send ACMD51 SD_APP_SEND_SCR with argument as 0 */
    cmd.idx = SD_CMD_SD_APP_SEND_SCR;
    cmd.res = SD_RESP_SHORT;
    cmd.arg = 0;
    PHAL_SDIO_SendCommand(&cmd);

    errorstatus = CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);

    if (errorstatus != SD_OK) {
        return (errorstatus);
    }

    while (!(SDIO->STA & (SDIO_STA_RXOVERR | SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_DBCKEND | SDIO_STA_STBITERR))) {
        if (SDIO->STA & SDIO_STA_RXDAVL) {
            *(tempscr + index) = SDIO->FIFO;
            index++;
        }
    }

    if (SDIO->STA & SDIO_STA_DTIMEOUT) {
        SDIO->ICR   = (SDIO_ICR_DTIMEOUTC);
        errorstatus = SD_DATA_TIMEOUT;
        return (errorstatus);
    } else if (SDIO->STA & SDIO_STA_DCRCFAIL) {
        SDIO->ICR   = (SDIO_ICR_DCRCFAILC);
        errorstatus = SD_DATA_CRC_FAIL;
        return (errorstatus);
    } else if (SDIO->STA & SDIO_STA_RXOVERR) {
        SDIO->ICR   = (SDIO_ICR_RXOVERRC);
        errorstatus = SD_RX_OVERRUN;
        return (errorstatus);
    } else if (SDIO->STA & SDIO_STA_STBITERR) {
        SDIO->ICR   = (SDIO_ICR_STBITERRC);
        errorstatus = SD_START_BIT_ERR;
        return (errorstatus);
    }

    /*!< Clear all the static flags */
    SDIO->ICR = (SDIO_STATIC_FLAGS);

    *(pscr + 1) = ((tempscr[0] & SD_0TO7BITS) << 24) | ((tempscr[0] & SD_8TO15BITS) << 8) | ((tempscr[0] & SD_16TO23BITS) >> 8) | ((tempscr[0] & SD_24TO31BITS) >> 24);

    *(pscr) = ((tempscr[1] & SD_0TO7BITS) << 24) | ((tempscr[1] & SD_8TO15BITS) << 8) | ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);

    return (errorstatus);
}

//---------------------------------------------------------
// Lower level functions and IRQs
//---------------------------------------------------------

/**
 * @brief  Allows to process all the interrupts that are high.
 */
void SDIO_IRQHandler(void) {
    if (SDIO->STA & SDIO_STA_DATAEND) {
        TransferError = SD_OK;
        SDIO->ICR     = SDIO_ICR_DATAENDC;
        TransferEnd   = 1;
        SD_LOG("SDIO IRQ : TransferEnd = 1, OK\r\n");
    }
    else if (SDIO->STA & SDIO_STA_DCRCFAIL) {
        SDIO->ICR = SDIO_ICR_DCRCFAILC;
        if (SDIO->STA & SDIO_STA_DBCKEND) {
            SDIO->ICR   = SDIO_ICR_DBCKENDC;
            TransferEnd = 1;
        } else {
            TransferError = SD_DATA_CRC_FAIL;
            SD_LOG("SDIO IRQ : SD_DATA_CRC_FAIL\r\n");
        }
    } else if (SDIO->STA & SDIO_STA_DTIMEOUT) {
        SDIO->ICR     = SDIO_ICR_DTIMEOUTC;
        TransferError = SD_DATA_TIMEOUT;
        SD_LOG("SDIO IRQ : SD_DATA_TIMEOUT\r\n");
    } else if (SDIO->STA & SDIO_STA_RXOVERR) {
        SDIO->ICR     = SDIO_ICR_RXOVERRC;
        TransferError = SD_RX_OVERRUN;
        SD_LOG("SDIO IRQ : SD_RX_OVERRUN\r\n");
    } else if (SDIO->STA & SDIO_STA_TXUNDERR) {
        SDIO->ICR     = SDIO_ICR_TXUNDERRC;
        TransferError = SD_TX_UNDERRUN;
        SD_LOG("SDIO IRQ : SD_TX_UNDERRUN\r\n");
    } else if (SDIO->STA & SDIO_STA_STBITERR) {
        SDIO->ICR     = SDIO_ICR_STBITERRC;
        TransferError = SD_START_BIT_ERR;
        SD_LOG("SDIO IRQ : SD_START_BIT_ERR\r\n");
    }
    // Disable the following interrupts
    SDIO->MASK &= ~(SDIO_MASK_DCRCFAILIE | SDIO_MASK_DTIMEOUTIE | SDIO_MASK_DATAENDIE | SDIO_MASK_TXFIFOHEIE | SDIO_MASK_RXFIFOHFIE | SDIO_MASK_TXUNDERRIE | SDIO_MASK_RXOVERRIE | SDIO_MASK_STBITERRIE);
}

void DMA2_Stream6_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
    // Check what triggered interrupt
    if (DMA2->HISR & DMA_HISR_TCIF6) {
        // Clear interrupt flags (transfer complete and error)
        DMA2->HIFCR      = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CFEIF6;

        // Old end of transfer flag
        DMAEndOfTransfer = 0x01;

        if(sd_task_handle != NULL) {
            vTaskNotifyGiveFromISR(sd_task_handle, &xHigherPriorityTaskWoken);
            sd_task_handle = NULL;
            //portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
            // i probably should figure out why this is bad for wait read idk
        }
    }
}

#define SDIO_TXDMA_CONFIG(tx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(SDIO->FIFO), \
        .mem_addr         = (uint32_t)(tx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b1, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b10, \
        .periph_size      = 0b10, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 6, \
        .periph           = DMA2, \
        .stream           = DMA2_Stream6}
static dma_init_t sdio_tx_config = SDIO_TXDMA_CONFIG(NULL, 3);

/**
 * @brief  Configures the DMA 2 Channel 6 for SDIO Tx request.
 * @param  BufferSRC: pointer to the source buffer
 * @param  BufferSize: buffer size
 * @retval None
 */
void SD_LowLevel_DMA_TxConfig(const uint32_t* BufferSRC, uint32_t BufferSize) {
    // TODO: convert DMA stuff to PHAL
    sdio_tx_config.mem_addr = (uint32_t)BufferSRC;
    PHAL_initDMA(&sdio_tx_config);
    sdio_tx_config.stream->FCR &= ~(DMA_SxFCR_FTH_Msk);
    sdio_tx_config.stream->FCR |= DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;
    // peripheral flow control, and burst transfer of 4 beats
    sdio_tx_config.stream->CR |= DMA_SxCR_PFCTRL | DMA_SxCR_MBURST_0 | DMA_SxCR_PBURST_0 | DMA_SxCR_TCIE;
    PHAL_DMA_startTxfer(&sdio_tx_config);

    // TODO: if it doesn't work, look at why memory size is byte, peirph size is word, and fifo threshold half full, memory burst single
}

#define SDIO_RXDMA_CONFIG(rx_addr_, priority_) \
    { \
        .periph_addr      = (uint32_t)&(SDIO->FIFO), \
        .mem_addr         = (uint32_t)(rx_addr_), \
        .tx_size          = 1, \
        .increment        = false, \
        .circular         = false, \
        .dir              = 0b0, \
        .mem_inc          = true, \
        .periph_inc       = false, \
        .mem_to_mem       = false, \
        .priority         = (priority_), \
        .mem_size         = 0b10, \
        .periph_size      = 0b10, \
        .tx_isr_en        = true, \
        .dma_chan_request = 0b0100, \
        .stream_idx       = 6, \
        .periph           = DMA2, \
        .stream           = DMA2_Stream6}

static dma_init_t sdio_rx_config = SDIO_RXDMA_CONFIG(NULL, 3);

/**
 * @brief  Configures the DMA2 Channel4 for SDIO Rx request.
 * @param  BufferDST: pointer to the destination buffer
 * @param  BufferSize: buffer size
 * @retval None
 */
// DMA writes rx'd blocks into BufferDST async
// cppcheck-suppress constParameterPointer
void SD_LowLevel_DMA_RxConfig(uint32_t* BufferDST, uint32_t BufferSize) {
    // TODO: convert DMA stuff to PHAL
    sdio_rx_config.mem_addr = (uint32_t)BufferDST;
    PHAL_initDMA(&sdio_rx_config);
    // sdio_rx_config.tx_size = BufferSize;
    // Note: when using peripheral flow control, SDIO signals
    // the end of the transfer, meaning SxNDTR is don't care
    // view table 48 on allowed FIFO configs
    // fifo mode enable with full threshold (do BEFORE enabling burst)
    sdio_rx_config.stream->FCR |= DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;
    // peripheral flow control, and burst transfer of 4 beats
    sdio_rx_config.stream->CR |= DMA_SxCR_PFCTRL | DMA_SxCR_MBURST_0 | DMA_SxCR_PBURST_0;

    PHAL_DMA_startTxfer(&sdio_rx_config);
}

void SD_DeInit(void) {
    /*!< Disable SDIO Clock */
    // SDIO_ClockCmd(DISABLE);
    SDIO->CLKCR &= ~(SDIO_CLKCR_CLKEN);

    /*!< Set Power State to OFF */
    // SDIO_SetPowerState(SDIO_PowerState_OFF);
    SDIO->POWER &= ~(SDIO_POWER_PWRCTRL);

    /*!< DeInitializes the SDIO peripheral */
    // SDIO_DeInit();

    /* Disable the SDIO APB2 Clock */
    RCC->APB2ENR &= ~RCC_APB2ENR_SDIOEN;
}
