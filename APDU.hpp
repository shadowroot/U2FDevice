#pragma once

enum APDU : uint8_t
{
	U2F_REG  = 0x01,
	U2F_AUTH = 0x02,
	U2F_VER  = 0x03
};

enum APDU_STATUS : uint16_t
{
	SW_NO_ERROR                 = 0x9000,
	SW_WRONG_LENGTH             = 0x6700,
	SW_CONDITIONS_NOT_SATISFIED = 0x6985,
	SW_WRONG_DATA               = 0x6A80,
	SW_INS_NOT_SUPPORTED        = 0x6D00,
	SW_COMMAND_NOT_ALLOWED      = 0x6E00,
};
