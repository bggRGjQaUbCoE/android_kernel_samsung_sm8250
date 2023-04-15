#ifndef _FTS_SPEC_H_
#define  _FTS_SPEC_H_

/* No use in winner project */
const short original_rawdata_rx_gap[] =
{
};

const short high_freq_raw_spec_max = 1200;
const short high_freq_raw_spec_min = 65;
const short high_freq_raw_tx_spec_max = 165;
const short high_freq_raw_tx_spec_min = -165;
const short high_freq_raw_rx_spec_max = 165;
const short high_freq_raw_rx_spec_min = -165;
const short micro_short_spec_max = 105;
const short micro_short_spec_min = -105;
const short miscal_spec_max = 300;

const short ref_micro_short_tx_gap[] =
{
	20, -10, -9, -8, -8, -8, -6, -6, -8, -7, -7, -7, -6, -6, -8, -8, -9, -9, -9, -8, -8, -9, -10, -9, -8, -9, -7, -7, -8, -9, -11, -17, -37, -33, 28, 0, -11, -18, -33, -14,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, -1, -2, -8, -27, -30, 37, 10, -2, -8, -17,
	0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, -1, -7, -26, -32, 37, 11, -1, -6,
	0, 1, 1, 1, 1, 0, 0, 1, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, -1, -6, -23, -34, 36, 11, -1,
	1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, -1, -5, -22, -36, 37, 13,
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -6, -21, -40, 36,
	-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, -1, -1, 0, 0, 0, 0, -1, 0, -1, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -1, -1, -6, -22, -44,
	1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, -1, -4, -14,
	4, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, -1, -2,
	2, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 0,
	22, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, -1, 0, -2, -1, 0, 0,
	-18, 30, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
	53, -30, 3, 0, 1, -1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, 0, -1, -1, -1, -1, -1, 0, -1, -1, 0,
	216, 93, 40, 32, 27, 51, 69, 48, 16, 7, 5, 4, 3, 2, 1, 1, 1, 0, -1, -2, -2, -2, -1, -2, -3, -2, -2, -3, -4, -4, -4, -4, -4, -4, -5, -5, -6, -7, -9, -67,
	-103, -42, -4, 3, 9, 36, -24, -33, -8, -2, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
	-34, -12, 1, 18, 35, -29, -30, -8, -2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
	-17, -1, 12, 39, -31, -6, -7, 12, -1, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, -1, -35,
	-10, 10, 24, -38, -26, -30, 26, -13, 1, -1, -1, -1, -1, -1, 0, 0, 0, -1, 0, -1, -1, 0, 0, -1, -1, -1, 0, 0, -1, -1, -1, -1, 0, -1, -1, 0, -2, -2, -160, 0,
	9, 35, -38, -30, -7, -5, -11, 3, 2, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 5, 0, 0,
	36, -30, -22, -6, -1, 7, -6, 23, 0, 6, 1, 1, 1, 1, 1, 1, 12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 2, 1, 1, 0, 0, 0, 0,
	-43, -34, -3, -7, 2, -16, -9, -25, 0, 1, 2, 2, 3, 2, 3, 3, -9, 3, 2, 14, 2, 2, 2, 3, 2, 2, 3, 3, 2, 3, 7, 20, 2, 9, 19, 2, 18, 12, 0, 0,
	-20, -4, -1, 0, 0, 1, 0, 0, 0, -4, 0, 0, 0, 1, 0, 0, 0, 0, 0, -10, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, -9, 0, 2, -4, 0, -1, -3, 0, 0,
	-4, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, -1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -2, -5, 1, 4, 1, 1, 6, 1, 0, 0,
	0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, -1, -3, 1, -11, -11, 6, -9, 11, 0, 0,
	1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, -3, -6, 6, 0, 0,
	0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 2, 0, -1, 4, 0, 0,
	-68, 3, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 4, 4, 3, 3, -1, 0, 0
};

const short ref_high_freq_correlation[] =
{
	235,271,268,268,271,268,264,264,266,264,269,264,266,271,272,264,260,257,259,254,259,258,266,258,256,252,250,259,253,249,243,243,243,247,242,235,232,239,193,179,
	207,224,224,220,228,217,216,223,219,221,221,220,216,219,218,218,213,205,206,207,208,207,214,207,207,204,202,207,209,201,198,197,185,193,189,193,192,189,139,139,
	205,219,216,215,209,217,213,211,207,213,215,216,209,213,212,210,208,200,203,204,197,205,209,206,199,198,199,196,205,190,187,186,180,178,186,189,189,180,133,133,
	195,210,204,208,204,208,205,205,202,208,206,200,198,208,207,203,195,190,185,192,186,190,191,186,191,184,182,186,193,182,180,178,168,170,170,172,182,172,119,128,
	181,200,201,193,189,193,194,189,188,192,189,190,189,192,191,181,181,173,171,173,175,174,177,178,174,168,174,172,179,166,164,167,157,159,155,153,164,162,114,112,
	167,176,175,171,172,174,170,169,169,177,170,168,167,172,168,169,160,155,154,161,158,159,165,166,160,159,152,156,162,156,147,148,140,143,140,142,142,143,103,100,
	147,155,155,150,156,149,150,150,146,156,149,145,146,152,152,146,140,135,132,136,135,141,143,142,137,136,129,137,140,126,129,125,119,119,119,115,120,120,75,82,
	137,150,151,141,141,142,144,136,132,140,139,136,132,146,145,134,130,120,122,125,124,130,128,128,130,128,122,122,126,121,118,119,111,111,108,116,112,109,70,72,
	135,144,140,141,143,140,134,133,129,141,136,134,130,136,132,133,122,120,117,124,120,124,130,125,122,118,119,122,125,122,115,116,113,109,106,109,109,106,68,70,
	141,148,150,146,143,140,144,134,136,142,137,131,136,148,144,135,134,126,121,129,123,129,128,131,125,126,124,126,129,123,120,115,111,117,112,113,113,111,67,66,
	139,144,148,153,145,144,138,143,137,143,133,133,131,137,139,136,127,121,124,124,121,125,136,123,121,120,118,123,127,127,122,116,108,112,108,109,110,108,64,70,
	136,141,139,137,139,133,134,134,125,136,126,124,127,134,132,125,122,111,117,118,116,127,129,121,124,111,113,118,124,115,110,112,104,104,101,104,102,106,60,60,
	136,148,147,145,142,134,136,136,131,139,135,132,131,138,140,129,127,120,117,123,121,122,124,120,124,119,114,119,127,120,113,113,104,107,106,105,112,108,63,62,
	141,145,141,143,143,139,136,138,133,134,133,129,131,134,132,126,126,117,118,120,121,128,126,121,121,116,114,123,122,114,109,112,103,109,107,104,109,109,61,62,
	136,119,113,111,111,114,120,115,113,124,113,120,114,128,122,120,130,115,117,123,122,126,132,126,133,133,126,139,146,139,132,136,133,133,131,138,146,146,94,88,
	123,106,104,108,108,114,110,111,105,115,112,109,112,121,120,115,113,108,110,121,116,122,122,122,123,126,121,136,135,135,129,130,128,132,133,134,142,143,95,89,
	119,109,109,110,110,117,109,107,107,112,115,111,115,123,127,116,117,111,115,119,120,123,130,127,128,125,129,137,140,136,132,136,131,130,129,137,144,144,96,91,
	116,111,109,113,112,108,107,106,108,113,112,108,114,123,130,118,117,113,119,127,121,122,129,129,125,130,134,134,146,137,132,130,129,133,131,136,144,145,96,82,
	107,101,109,101,102,99,106,102,99,111,109,111,106,121,117,115,113,107,108,119,118,115,123,120,125,124,125,134,132,131,129,127,125,132,125,135,135,142,53,-73,
	100,103,100,97,94,98,97,100,98,105,106,107,108,116,114,107,109,105,107,116,111,115,117,118,118,120,121,128,133,132,127,127,122,131,120,127,135,141,-68,-71,
	103,97,92,94,96,97,97,101,97,105,104,104,103,115,113,106,110,102,105,116,107,116,120,123,119,121,123,129,130,129,120,124,124,127,126,127,133,142,-81,-75,
	108,103,102,105,101,106,107,108,111,117,113,111,114,124,129,123,123,112,118,127,121,122,135,132,136,132,135,141,143,136,139,142,137,139,140,144,145,156,-71,-76,
	100,101,101,100,106,108,109,108,108,117,118,120,119,130,129,124,125,118,120,130,123,129,135,132,137,138,135,144,148,145,140,142,137,145,146,149,146,155,-77,-73,
	107,101,106,102,106,109,108,116,113,121,117,117,119,128,133,130,128,125,128,135,127,128,140,139,144,143,137,145,155,146,146,144,139,144,141,146,155,157,-75,-74,
	104,102,105,108,109,107,109,117,116,126,123,124,123,136,132,134,129,125,129,135,130,140,141,140,141,141,147,152,150,149,147,146,140,146,139,152,149,158,-78,-74,
	99,99,103,101,109,110,108,113,110,124,118,124,124,136,133,134,131,127,131,138,129,137,145,142,142,139,142,152,154,149,147,142,140,144,139,147,151,156,-82,-74,
	102,99,107,107,113,116,118,115,118,125,125,124,129,137,134,135,137,129,135,141,137,139,146,147,147,147,151,147,159,153,149,153,141,147,146,148,150,158,-80,-73,
	86,106,108,113,115,115,122,123,125,135,134,139,132,150,149,149,146,132,144,145,149,154,154,160,159,156,155,161,174,166,158,157,153,154,154,153,158,164,-78,-74
};

const short ref_high_freq_rawdata_tx_gap[] =
{
	51,26,25,26,26,27,28,29,26,26,28,29,28,29,26,25,26,26,23,25,26,25,25,24,25,25,27,25,23,23,21,11,-15,-2,63,31,20,15,-1,27,
	1,2,1,2,2,2,2,1,2,2,0,1,3,2,2,2,2,1,2,2,2,1,2,2,3,2,3,2,1,2,1,-2,-7,-34,-29,43,13,1,-7,-12,
	4,5,5,4,4,5,5,4,5,5,5,3,5,5,5,4,4,3,4,3,3,3,4,2,2,2,2,4,4,3,3,5,2,-4,-29,-31,46,16,2,-4,
	1,1,2,2,1,1,1,2,1,1,2,2,2,1,2,1,1,1,1,2,2,2,1,2,1,2,0,2,1,2,1,1,1,-1,-6,-29,-37,43,13,-2,
	5,4,6,3,6,7,4,4,5,5,5,5,3,5,5,5,6,5,6,7,7,7,5,7,7,7,7,6,6,6,6,6,5,4,5,-2,-24,-37,45,17,
	1,1,1,3,1,1,2,2,2,2,1,1,2,0,3,1,0,0,2,2,1,1,1,1,1,1,1,1,2,1,0,1,1,0,-1,-1,-5,-25,-45,41,
	1,1,1,1,2,1,2,2,2,2,2,3,0,2,0,3,3,3,1,1,1,1,1,2,1,2,1,1,1,1,2,1,3,2,0,0,-1,-6,-26,-51,
	0,-1,0,0,1,-1,-2,-1,0,0,0,-1,1,0,0,0,1,0,0,0,0,0,0,0,0,-1,2,0,0,0,0,0,1,0,0,-1,-1,-1,-4,-20,
	7,5,5,5,4,3,5,5,5,5,4,3,4,4,4,4,5,5,5,5,4,5,5,4,4,5,4,4,4,4,4,5,4,4,6,3,3,4,1,-1,
	1,3,0,0,0,0,1,1,1,0,0,1,-1,1,1,1,0,0,1,0,0,1,0,0,1,1,0,1,1,1,1,0,1,1,0,-1,0,-1,0,-2,
	25,2,3,3,3,2,3,2,2,2,2,2,4,2,3,2,2,2,2,3,3,3,2,2,2,2,3,2,2,1,1,2,2,2,2,3,2,3,1,-1,
	-17,34,-1,-1,-1,1,-1,0,-1,0,1,-1,0,-1,-1,-1,0,0,-1,1,1,-2,-1,-1,-1,-1,-2,-1,-1,-1,-1,-2,-1,-1,-1,-2,-1,-1,-1,-2,
	54,-31,4,1,2,0,2,1,1,2,2,2,2,1,1,2,1,2,2,1,1,1,0,1,1,1,1,1,1,1,0,0,-1,1,0,1,1,1,-1,1,
	179,88,41,35,31,57,77,57,19,8,7,6,3,1,1,0,0,-1,-1,-2,-3,-3,-3,-3,-3,-5,-6,-5,-6,-6,-8,-8,-9,-9,-10,-11,-10,-12,-10,-59,
	-87,-36,1,5,13,43,-22,-37,-8,0,1,2,2,3,2,1,2,1,1,3,2,2,2,3,1,2,2,1,2,1,2,2,3,4,3,2,0,3,2,2,
	-29,-10,3,21,41,-29,-35,-9,-2,0,0,0,0,0,0,1,0,1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,2,1,0,2,2,0,0,1,
	-17,1,14,45,-32,-9,-8,13,0,-1,0,1,0,1,1,0,1,1,0,0,0,1,1,1,1,1,1,1,1,2,2,0,0,0,1,1,0,-1,1,-28,
	-11,11,28,-41,-32,-33,29,-14,1,-1,0,-2,0,-1,-1,0,1,-1,0,-1,0,-2,-1,-1,-1,0,-1,0,1,0,0,-2,-1,-1,0,-1,-1,0,0,0,
	9,41,-41,-34,-7,-5,-11,4,2,2,1,1,1,1,2,1,1,2,1,1,1,1,1,1,1,1,2,2,1,1,1,2,2,1,1,-1,0,8,0,0,
	40,-32,-25,-7,0,8,-6,25,0,7,1,3,1,2,2,2,13,1,1,2,2,2,1,2,2,2,1,2,1,1,1,1,1,1,2,2,1,2,0,0,
	-52,-40,-4,-6,3,-15,-8,-25,1,-2,4,4,3,4,4,4,-9,5,4,4,4,4,4,4,4,4,4,3,3,4,9,23,3,11,22,4,22,14,0,0,
	-20,-4,0,0,1,2,1,2,2,0,1,1,2,1,1,1,2,1,2,1,1,1,2,1,2,1,1,1,2,1,-1,-9,1,3,-3,1,-1,-3,0,0,
	-5,-1,-1,0,0,-1,0,0,-1,-1,-1,0,1,1,-1,-1,-1,0,-1,-1,-1,-1,0,-1,-2,-1,0,-1,-1,-2,-3,-7,0,3,-1,0,7,-1,0,0,
	-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-1,-1,-3,-4,-1,-12,-14,6,-11,9,0,0,
	2,2,2,2,2,2,2,2,2,1,2,1,1,1,1,2,2,2,2,3,1,2,3,1,2,0,1,2,2,2,1,2,2,2,2,-3,-6,9,0,0,
	0,0,0,0,0,0,0,0,-1,-1,-1,0,-1,0,1,0,0,1,1,1,2,1,2,2,1,2,1,1,1,2,2,2,2,2,2,1,-2,5,0,0,
	-60,11,9,9,9,8,9,8,9,8,9,9,9,9,9,9,10,9,9,8,10,9,9,9,10,10,9,9,9,10,10,10,10,10,10,9,9,8,0,0
};

const short ref_high_freq_rawdata_rx_gap[] =
{
	54,-3,0,1,-3,-1,-2,4,-4,3,-4,2,-1,5,0,-1,1,-1,-1,2,5,0,2,0,-3,-3,4,5,-1,-1,16,30,26,-39,-5,-2,8,4,11,
	28,-3,1,0,-2,1,-2,1,-4,4,-2,2,0,2,-1,0,1,-4,1,3,4,-1,1,1,-2,-1,2,3,-1,-3,7,4,39,26,-37,-13,2,-12,39,
	29,-4,2,0,-2,1,-2,3,-4,2,-1,3,-1,2,0,0,0,-3,1,3,4,0,1,2,-3,0,1,3,-1,-4,3,-2,12,30,35,-43,-11,-19,34,
	30,-4,1,0,-1,1,-3,3,-4,2,-3,5,0,1,-1,-1,0,-3,0,3,4,1,-1,2,-4,1,3,3,-2,-5,5,-4,7,5,33,34,-41,-33,29,
	30,-3,2,-1,-1,1,-2,3,-3,2,-3,5,-1,2,-1,-1,0,-4,1,4,3,0,1,1,-3,-1,4,3,-1,-6,5,-4,6,-1,11,26,39,-62,14,
	28,-1,-2,3,-1,-1,-3,4,-4,3,-3,3,1,2,-1,0,-1,-3,2,3,3,-1,3,1,-2,-2,3,3,-1,-5,5,-6,5,-1,5,4,25,20,-14,
	28,-1,0,1,0,0,-3,4,-4,2,-3,4,-1,4,-3,-1,0,-2,1,3,3,0,2,1,-2,-2,3,4,-2,-5,5,-6,4,-1,5,0,5,0,72,
	28,-1,-1,2,-1,1,-3,3,-3,2,-2,2,1,2,-1,0,0,-4,2,3,3,0,3,0,-1,-3,3,3,-2,-4,5,-5,3,-3,6,-1,0,-20,47,
	28,0,0,3,-3,0,-3,4,-3,2,-3,3,0,2,-1,1,-1,-4,1,3,2,0,2,0,-2,0,1,4,-2,-4,5,-4,2,-3,5,-1,-1,-22,30,
	26,0,-1,2,-3,2,-3,4,-3,1,-4,4,0,2,-1,1,0,-4,1,2,3,0,2,0,-2,-1,2,3,-2,-4,6,-5,2,-2,3,-1,-1,-24,28,
	28,-3,-1,2,-3,2,-3,4,-4,1,-3,3,2,2,0,0,0,-3,1,2,3,0,2,1,-2,-1,2,3,-2,-4,5,-4,2,-3,3,-1,-2,-24,27,
	4,-2,-1,2,-4,4,-4,4,-4,1,-3,4,0,2,-1,0,0,-3,1,2,4,-1,2,1,-2,0,1,3,-2,-4,6,-5,3,-2,3,-1,-1,-26,26,
	55,-37,-1,1,-1,2,-3,4,-4,2,-5,5,-1,2,0,0,0,-4,2,2,1,0,2,1,-2,-1,2,3,-2,-4,5,-4,2,-3,3,-1,-1,-26,26,
	-31,-1,-3,2,-3,4,-4,4,-3,2,-5,5,-1,2,1,-1,0,-3,2,2,2,-1,2,1,-2,-1,2,3,-3,-4,5,-4,3,-3,3,-1,-1,-28,28,
	-121,-49,-9,-3,23,24,-24,-34,-14,1,-6,2,-3,1,0,-1,-1,-3,1,1,2,-1,2,1,-4,-2,2,2,-2,-6,5,-5,3,-4,3,-1,-3,-27,-21,
	-70,-13,-4,5,53,-41,-39,-5,-6,2,-5,3,-3,1,-1,0,-2,-4,3,0,2,-1,2,-1,-3,-2,1,3,-3,-6,6,-5,4,-4,1,-3,0,-27,-21,
	-51,1,14,25,-17,-47,-12,2,-4,2,-5,3,-3,1,-1,-1,-1,-3,3,-1,3,-2,3,-1,-3,-2,1,3,-2,-6,7,-5,3,-5,3,-3,-1,-28,-20,
	-33,14,45,-52,6,-46,8,-11,-5,2,-4,1,-1,0,-1,-1,-1,-4,2,0,3,-2,3,-1,-3,-2,1,2,-1,-6,5,-5,3,-5,4,-4,-2,-26,-48,
	-11,31,-24,-43,5,16,-35,4,-6,3,-6,3,-2,1,0,0,-2,-3,2,0,2,-1,3,-1,-2,-2,2,3,-2,-6,4,-4,3,-4,3,-3,-1,0,0,
	20,-51,-17,-16,7,9,-20,3,-7,2,-6,3,-1,1,-1,0,-1,-4,2,0,2,-1,2,-1,-2,-1,1,2,-2,-6,5,-4,3,-5,2,-2,7,0,0,
	-51,-44,1,-9,15,-5,12,-22,0,-4,-5,2,-1,2,-1,11,-14,-3,2,0,2,-2,3,0,-2,-2,1,2,-3,-6,5,-5,3,-4,2,-3,8,0,0,
	-39,-8,-2,1,-4,2,-5,4,-3,2,-5,1,0,1,-1,-1,-1,-4,2,0,3,-2,3,0,-3,-1,0,2,-1,-1,18,-24,11,7,-16,15,0,0,0,
	-23,-4,-2,1,-2,1,-4,4,-4,2,-5,2,-1,2,-1,-1,-1,-3,2,0,2,-1,2,1,-3,-1,1,2,-2,-2,10,-15,12,1,-12,13,-2,0,0,
	-19,-4,-1,1,-3,1,-3,3,-4,2,-4,3,-1,0,-1,-1,-1,-3,1,0,3,-1,2,0,-2,-1,0,2,-3,-3,5,-7,15,-2,-11,20,-9,0,0,
	-19,-3,-1,1,-3,0,-3,3,-4,2,-4,3,-1,0,-1,-1,-1,-3,0,0,3,-1,3,0,-3,0,-1,3,-3,-5,4,-4,3,-3,8,4,11,0,0,
	-18,-4,0,0,-3,1,-4,3,-4,3,-5,3,-1,0,0,-1,0,-3,1,-1,3,-1,1,0,-4,0,0,3,-3,-6,5,-5,3,-3,3,1,25,0,0,
	-18,-4,0,0,-3,1,-4,3,-4,2,-5,3,-1,0,-1,-1,0,-3,2,0,2,0,1,-1,-3,-1,0,3,-2,-5,4,-4,3,-3,3,-2,31,0,0,
	53,-5,-1,0,-3,1,-4,3,-4,3,-5,3,-1,1,-1,0,-1,-3,1,1,2,0,1,0,-3,-1,0,3,-2,-5,5,-5,4,-3,2,-2,29,0,0
};

const short ref_cx2_tx_gap[] =
{
	5, 4, 3, 3, 3, 3, 3, 4, 4, 4, 2, 3, 3, 4, 3, 3, 2, 3, 4, 3, 3, 3, 3, 2, 3, 3, 3, 4, 2, 4, 3, 2, 1, 2, 5, 3, 3, 3, 1, 2,
	1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 2, 2, 0, -1, 2, 0, 0, 0, 2, 0, 1, 0, 1, 2, 0, 1, 0, 0, 1, 0, 0, 1, 0, -1, -1, 3, 1, 0, 0, 0,
	0, 1, 1, 0, 0, 1, 1, 0, -1, 1, 0, 0, 1, 1, -1, 1, 1, 1, -1, 1, 1, 1, 0, -1, 1, 0, 1, 1, 0, 1, 2, -1, 1, 0, -1, 0, 3, 3, 0, 0,
	0, -1, 0, 1, 0, 1, 2, 1, 1, 1, 1, 1, 1, 2, 2, 0, 1, 1, 1, 1, 0, 1, 2, 1, 1, 1, 1, 1, 2, 0, 0, 1, 0, 0, 0, -2, -1, 1, 2, 1,
	1, 2, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, -1, -1, 2, 1,
	1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, -1, -2, 3,
	0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, -3,
	0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, -1,
	0, 0, 0, 0, 0, -1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 2, 1, 1, 1, 0, 0, 0,
	1, 0, 0, 0, 1, 0, 0, 2, 1, 0, 1, 1, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0,
	1, 0, 1, 1, 0, 0, 1, 0, -1, 1, 0, 0, 1, -1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, -1, 0, 1, 1, 1, 0, 1,
	1, 2, 0, 0, 0, 1, -1, -1, 0, -1, 0, 1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 1, -1, 0, -1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0,
	2, -1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 2, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	10, 4, 3, 2, 2, 3, 4, 3, 2, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 1, -1, 0, 0, -1, 0, 0, -1, 1, 0, 0, 0, -1, -1, -3,
	-5, -1, -1, 2, 1, 2, 0, -2, -1, 0, -1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 2, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0,
	-1, 0, 2, 0, 2, -1, -2, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0,
	-1, 0, 0, 2, -2, 0, -1, 1, 1, 0, 0, 0, 1, 0, -1, 0, 1, 1, 0, 1, 0, 1, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -2,
	0, 1, 1, -2, -1, -1, 2, 0, 0, 0, 0, 1, -1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -8, 0,
	0, 2, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0, 0, -1, 0, -1, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	3, -1, -1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, -1, 2, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0,
	-3, -2, 0, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, -1, 0, 1, 1, 1, 1, 0, 1, 2, 1, 0, 3, 1, 2, 2, 0, 0,
	-1, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, -1, 0, 1, -1, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, -1, 0, -1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 0,
	0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, -1, 0, 0, 0, 1, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0,
	0, -1, -1, 0, 0, 0, -1, -1, 0, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 1, 0, 1, 0, 0,
	0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, -1, 0, 2, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0,
	-3, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 1, 1, -1, 0, -1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0
};

const short ref_cx2_rx_gap[] =
{
	2, 0, 0, 0, 0, 0, -1, 1, -1, 1, -1, 1, 0, 0, 1, 0, -1, -1, 1, 0, 0, 0, 1, -1, 0, 0, 0, 1, -1, 0, 2, 1, 1, -2, 0, 0, 0, 2, 0,
	1, -1, 0, 0, 0, 0, 0, 1, -1, -1, 0, 1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 1, 0, 2, 1, -2, 0, 0, 0, 1,
	0, 0, 0, 0, -1, 0, 1, 1, -2, 1, 0, -1, 0, 2, -1, -1, 0, 2, -2, 1, -1, 1, 1, -2, 1, -1, 1, 0, 0, -1, 2, -1, 1, 1, 2, -2, -1, 0, 1,
	1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 1, -1, 1, 0, -1, 1, 0, 0, 3, 1, -1, -3, 1,
	0, 1, 0, -1, 1, 1, -1, 0, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 1, 2, 1, -2, 0,
	1, 0, 1, -1, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, -1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 1, -1, 0, -1, 0, 0, 0, 1, 0, 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, -2, 0, 4,
	2, 0, -1, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 2,
	2, 0, 0, 0, 1, -1, -1, 0, 0, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, -1, 0, 1, 0, 0, -2, 1,
	2, 0, 0, 0, 0, 0, -1, 1, 0, -1, 0, 1, -1, 2, -1, 0, 0, -1, 1, 0, 0, 1, 0, 0, -1, -1, 1, 0, 0, 0, 0, 0, 1, -1, 1, 0, -1, -2, 1,
	1, 0, 0, 1, -1, 0, 1, 0, -1, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1, -1, 1, -1, 0, 0, 1, -1, 0, 1, -1, -1, 0,
	0, 1, 0, 0, -1, 1, 0, -1, 1, -1, 0, 1, -1, 1, 0, -1, 1, -1, 0, 0, 0, 2, -1, 0, 0, -1, 1, 0, 0, -1, 1, 0, -1, 0, 1, 1, -1, -2, 1,
	1, -1, 0, 0, 0, -1, 0, 0, 0, 0, 1, -1, 0, 1, 0, 0, 0, -1, 0, 0, 1, 0, 0, -1, 1, 0, -1, 1, 0, -1, 1, 0, -1, 1, 0, 1, -2, -1, 1,
	-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 1, -1, -2, 1,
	-8, -1, -1, 0, 1, 1, -1, -1, -1, 0, -1, 1, 0, 0, -1, 0, 0, 0, 0, -1, 0, 1, 0, -1, 2, -2, 1, 0, -1, 0, 1, -1, 2, -1, 0, 1, -2, -2, -1,
	-4, -1, 2, -1, 2, -1, -3, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 2, -1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 1, -1, 0, 1, -1, -3, -1,
	-3, 1, 0, 1, -1, -2, -1, 0, 0, 0, 0, 0, 0, 1, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0, 0, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 1, -1, -2, -2,
	-2, 1, 2, -3, 1, -3, 1, 0, -1, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, -1, 1, 0, 0, -1, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, -4,
	-1, 1, -1, -2, 1, 0, -1, 0, -1, 0, 1, -1, 0, 0, 1, -1, 0, 1, -1, 0, 0, 1, -1, 0, -1, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, 0,
	1, -2, -1, -1, 1, 0, -1, 0, -1, 0, 0, 0, 1, -1, 1, -1, 0, 0, 0, -1, 0, 2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 1, 0, 0,
	-3, -2, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 0, 0, 0, 0, -1, 1, -1, 0, 0, 0, 2, -2, -1, 0, 1, -1, 1, -1, 0, 0, 1, -1, 0, 0, 0, 0, 0,
	-2, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, -1, 0, 0, 1, -1, 0, 0, 1, -1, 0, 2, -2, 1, 0, 0, 0,
	-1, 0, 0, 0, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, -1, 1, -1, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 2, -2, 0, 0,
	-1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, -1, 1, -1, 0, 0, -1, 0, 0, 0, 2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, -1, 1, 0, 0, 0,
	0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 1, 0, 0, 0,
	-1, 0, 0, 0, 0, -1, 0, 1, -1, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
	0, -1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 2, -2, 0, 0, 0, 0, 0, 0, -1, 0, 1, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
	3, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0
};

const short fts_all_node_map[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0
};

const short fts_tx_gap_map[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const short fts_rx_gap_map[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
};

const short fts_miscal_map[] =
{
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0
};
#endif
