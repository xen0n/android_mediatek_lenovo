#ifndef _CUST_BATTERY_METER_TABLE_H
#define _CUST_BATTERY_METER_TABLE_H

#include <mach/mt_typedefs.h>

// ============================================================
// define
// ============================================================
#define BAT_NTC_10 1
#define BAT_NTC_47 0

#if (BAT_NTC_10 == 1)
#define RBAT_PULL_UP_R             16900	
//lenovo_sw liaohj modify for hw to 30k just use for after dvt2 2013-12-24  
//#define RBAT_PULL_DOWN_R		   27000	
#define RBAT_PULL_DOWN_R		   30000	
#endif

#if (BAT_NTC_47 == 1)
#define RBAT_PULL_UP_R             61900	
#define RBAT_PULL_DOWN_R		  100000	
#endif
#define RBAT_PULL_UP_VOLT          1800



// ============================================================
// ENUM
// ============================================================

// ============================================================
// structure
// ============================================================

// ============================================================
// typedef
// ============================================================
typedef struct _BATTERY_PROFILE_STRUC
{
    kal_int32 percentage;
    kal_int32 voltage;
} BATTERY_PROFILE_STRUC, *BATTERY_PROFILE_STRUC_P;

typedef struct _R_PROFILE_STRUC
{
    kal_int32 resistance; // Ohm
    kal_int32 voltage;
} R_PROFILE_STRUC, *R_PROFILE_STRUC_P;

typedef enum
{
    T1_0C,
    T2_25C,
    T3_50C
} PROFILE_TEMPERATURE;

// ============================================================
// External Variables
// ============================================================

// ============================================================
// External function
// ============================================================

// ============================================================
// <DOD, Battery_Voltage> Table
// ============================================================

//lenovo_sw liaohj modify for use alston ntc registers 2014-03-18
BATT_TEMPERATURE Batt_Temperature_Table[] = {
	{-20,67790},
	{-15,53460},
	{-10,42450},
	{ -5,33930},
	{  0,27280},
	{  5,22070},
	{ 10,17960},
	{ 15,14700},
	{ 20,12090},
	{ 25,10000},
	{ 30,8312},
	{ 35,6942},
	{ 40,5826},
	{ 45,4911},
	{ 50,4158},
	{ 55,3536},
	{ 60,3019}
};

/*lenovo_sw zhangrc2 change battery percent and internal resistance 2014-04-28 --- begin*/
// T0 -10C
BATTERY_PROFILE_STRUC battery_profile_t0[] =
{
   	{0  , 4328},
	{1  , 4315},
	{2  , 4302},
	{3  , 4282},
	{4  , 4273},
	{5  , 4263},
	{6  , 4246},
	{7  , 4238},
	{8  , 4229},
	{9  , 4231},
	{10 , 4219},
	{11 , 4206},
	{12 , 4197},
	{13 , 4188},
	{14 , 4170},
	{15 , 4162},
	{16 , 4153},
	{17 , 4137},
	{18 , 4129},
	{19 , 4120},
	{20 , 4112},
	{21 , 4103},
	{22 , 4090},
	{23 , 4084},
	{24 , 4078},
	{25 , 4065},
	{26 , 4055},
	{27 , 4045},
	{28 , 4018},
	{29 , 4004},
	{30 , 3990},
	{31 , 3979},
	{32 , 3967},
	{33 , 3950},
	{34 , 3943},
	{35 , 3936},
	{36 , 3925},
	{37 , 3921},
	{38 , 3916},
	{39 , 3908},
	{40 , 3904},
	{41 , 3900},
	{42 , 3896},
	{43 , 3891},
	{44 , 3883},
	{45 , 3879},
	{46 , 3874},
	{47 , 3865},
	{48 , 3861},
	{49 , 3857},
	{50 , 3853},
	{51 , 3849},
	{52 , 3842},
	{53 , 3838},
	{54 , 3834},
	{55 , 3827},
	{56 , 3824},
	{57 , 3820},
	{58 , 3814},
	{59 , 3811},
	{60 , 3808},
	{61 , 3805},
	{62 , 3802},
	{63 , 3797},
	{64 , 3795},
	{65 , 3792},
	{66 , 3788},
	{67 , 3787},
	{68 , 3786},
	{69 , 3782},
	{70 , 3781},
	{71 , 3779},
	{72 , 3777},
	{73 , 3775},
	{74 , 3772},
	{75 , 3771},
	{76 , 3769},
	{77 , 3765},
	{78 , 3764},
	{79 , 3762},
	{80 , 3760},
	{81 , 3757},
	{82 , 3753},
	{83 , 3751},
	{84 , 3749},
	{85 , 3744},
	{86 , 3742},
	{87 , 3739},
	{88 , 3733},
	{89 , 3731},
	{90 , 3728},
	{91 , 3724},
	{92 , 3716},
	{93 , 3713},
	{94 , 3711},
	{95 , 3706},
	{96 , 3704},
	{97 , 3701},
	{98 , 3699},
	{99 , 3696},
	{100, 3400}       
};      
        
// T1 0C 
BATTERY_PROFILE_STRUC battery_profile_t1[] =
{
   	{0  , 4343},
	{1  , 4328},
	{2  , 4318},
	{3  , 4307},
	{4  , 4288},
	{5  , 4280},
	{6  , 4272},
	{7  , 4255},
	{8  , 4247},
	{9  , 4239},
	{10 , 4224},
	{11 , 4216},
	{12 , 4208},
	{13 , 4193},
	{14 , 4178},
	{15 , 4171},
	{16 , 4163},
	{17 , 4148},
	{18 , 4141},
	{19 , 4133},
	{20 , 4117},
	{21 , 4110},
	{22 , 4103},
	{23 , 4090},
	{24 , 4086},
	{25 , 4082},
	{26 , 4071},
	{27 , 4052},
	{28 , 4037},
	{29 , 4021},
	{30 , 3993},
	{31 , 3983},
	{32 , 3972},
	{33 , 3956},
	{34 , 3950},
	{35 , 3943},
	{36 , 3932},
	{37 , 3928},
	{38 , 3923},
	{39 , 3914},
	{40 , 3905},
	{41 , 3901},
	{42 , 3896},
	{43 , 3885},
	{44 , 3881},
	{45 , 3876},
	{46 , 3867},
	{47 , 3863},
	{48 , 3858},
	{49 , 3850},
	{50 , 3846},
	{51 , 3842},
	{52 , 3835},
	{53 , 3829},
	{54 , 3826},
	{55 , 3822},
	{56 , 3816},
	{57 , 3814},
	{58 , 3811},
	{59 , 3806},
	{60 , 3804},
	{61 , 3801},
	{62 , 3796},
	{63 , 3794},
	{64 , 3792},
	{65 , 3788},
	{66 , 3784},
	{67 , 3783},
	{68 , 3782},
	{69 , 3778},
	{70 , 3777},
	{71 , 3776},
	{72 , 3773},
	{73 , 3772},
	{74 , 3770},
	{75 , 3767},
	{76 , 3765},
	{77 , 3763},
	{78 , 3760},
	{79 , 3755},
	{80 , 3752},
	{81 , 3748},
	{82 , 3742},
	{83 , 3739},
	{84 , 3735},
	{85 , 3728},
	{86 , 3724},
	{87 , 3719},
	{88 , 3710},
	{89 , 3707},
	{90 , 3703},
	{91 , 3699},
	{92 , 3696},
	{93 , 3694},
	{94 , 3692},
	{95 , 3685},
	{96 , 3672},
	{97 , 3658},
	{98 , 3591},
	{99 , 3536},
	{100, 3400}      
};           

// T2 25C
BATTERY_PROFILE_STRUC battery_profile_t2[] =
{
 	{0  , 4339},
	{1  , 4319},
	{2  , 4310},
	{3  , 4301},
	{4  , 4284},
	{5  , 4276},
	{6  , 4268},
	{7  , 4252},
	{8  , 4235},
	{9  , 4227},
	{10 , 4219},
	{11 , 4203},
	{12 , 4196},
	{13 , 4189},
	{14 , 4173},
	{15 , 4166},
	{16 , 4158},
	{17 , 4143},
	{18 , 4128},
	{19 , 4121},
	{20 , 4113},
	{21 , 4098},
	{22 , 4091},
	{23 , 4084},
	{24 , 4075},
	{25 , 4069},
	{26 , 4061},
	{27 , 4052},
	{28 , 4025},
	{29 , 4014},
	{30 , 4002},
	{31 , 3986},
	{32 , 3976},
	{33 , 3973},
	{34 , 3969},
	{35 , 3962},
	{36 , 3958},
	{37 , 3953},
	{38 , 3942},
	{39 , 3929},
	{40 , 3922},
	{41 , 3914},
	{42 , 3897},
	{43 , 3891},
	{44 , 3884},
	{45 , 3872},
	{46 , 3863},
	{47 , 3859},
	{48 , 3854},
	{49 , 3846},
	{50 , 3843},
	{51 , 3840},
	{52 , 3833},
	{53 , 3830},
	{54 , 3827},
	{55 , 3821},
	{56 , 3815},
	{57 , 3813},
	{58 , 3810},
	{59 , 3805},
	{60 , 3803},
	{61 , 3801},
	{62 , 3797},
	{63 , 3793},
	{64 , 3791},
	{65 , 3788},
	{66 , 3785},
	{67 , 3783},
	{68 , 3781},
	{69 , 3777},
	{70 , 3774},
	{71 , 3773},
	{72 , 3771},
	{73 , 3767},
	{74 , 3765},
	{75 , 3762},
	{76 , 3757},
	{77 , 3751},
	{78 , 3749},
	{79 , 3747},
	{80 , 3742},
	{81 , 3740},
	{82 , 3737},
	{83 , 3729},
	{84 , 3725},
	{85 , 3721},
	{86 , 3714},
	{87 , 3705},
	{88 , 3700},
	{89 , 3694},
	{90 , 3692},
	{91 , 3692},
	{92 , 3691},
	{93 , 3689},
	{94 , 3685},
	{95 , 3676},
	{96 , 3666},
	{97 , 3610},
	{98 , 3570},
	{99 , 3529},
	{100, 3400}	       
};     

// T3 50C
BATTERY_PROFILE_STRUC battery_profile_t3[] =
{
	{0  , 4347},
	{1  , 4334},
	{2  , 4320},
	{3  , 4309},
	{4  , 4298},
	{5  , 4287},
	{6  , 4276},
	{7  , 4265},
	{8  , 4254},
	{9  , 4243},
	{10 , 4232},
	{11 , 4222},
	{12 , 4211},
	{13 , 4201},
	{14 , 4190},
	{15 , 4180},
	{16 , 4169},
	{17 , 4158},
	{18 , 4147},
	{19 , 4137},
	{20 , 4127},
	{21 , 4107},
	{22 , 4097},
	{23 , 4086},
	{24 , 4077},
	{25 , 4067},
	{26 , 4058},
	{27 , 4048},
	{28 , 4039},
	{29 , 4030},
	{30 , 4020},
	{31 , 4010},
	{32 , 4002},
	{33 , 3994},
	{34 , 3986},
	{35 , 3978},
	{36 , 3970},
	{37 , 3962},
	{38 , 3954},
	{39 , 3945},
	{40 , 3936},
	{41 , 3926},
	{42 , 3914},
	{43 , 3902},
	{44 , 3893},
	{45 , 3884},
	{46 , 3877},
	{47 , 3869},
	{48 , 3864},
	{49 , 3858},
	{50 , 3853},
	{51 , 3847},
	{52 , 3842},
	{53 , 3836},
	{54 , 3832},
	{55 , 3828},
	{56 , 3824},
	{57 , 3819},
	{58 , 3811},
	{59 , 3808},
	{60 , 3804},
	{61 , 3801},
	{62 , 3797},
	{63 , 3794},
	{64 , 3790},
	{65 , 3788},
	{66 , 3785},
	{67 , 3782},
	{68 , 3779},
	{69 , 3776},
	{70 , 3772},
	{71 , 3765},
	{72 , 3758},
	{73 , 3753},
	{74 , 3747},
	{75 , 3743},
	{76 , 3739},
	{77 , 3736},
	{78 , 3732},
	{79 , 3729},
	{80 , 3725},
	{81 , 3722},
	{82 , 3719},
	{83 , 3715},
	{84 , 3710},
	{85 , 3705},
	{86 , 3699},
	{87 , 3694},
	{88 , 3689},
	{89 , 3683},
	{90 , 3676},
	{91 , 3676},
	{92 , 3675},
	{93 , 3674},
	{94 , 3672},
	{95 , 3669},
	{96 , 3665},
	{97 , 3614},
	{98 , 3565},
	{99 , 3515},
	{100, 3400}
		       
};           

// battery profile for actual temperature. The size should be the same as T1, T2 and T3
BATTERY_PROFILE_STRUC battery_profile_temperature[] =
{
  {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
        {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
        {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
        {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 }
};    

// ============================================================
// <Rbat, Battery_Voltage> Table
// ============================================================
// T0 -10C
R_PROFILE_STRUC r_profile_t0[] =
{
	{333, 4328},
	{713, 4315},
	{1093, 4302},
	{1108, 4282},
	{1104, 4273},
	{1100, 4263},
	{1093, 4246},
	{1089, 4238},
	{1085, 4229},
	{1120, 4231},
	{1099, 4219},
	{1078, 4206},
	{1077, 4197},
	{1075, 4188},
	{1065, 4170},
	{1062, 4162},
	{1058, 4153},
	{1048, 4137},
	{1043, 4129},
	{1038, 4120},
	{1032, 4112},
	{1025, 4103},
	{1025, 4090},
	{1026, 4084},
	{1028, 4078},
	{1028, 4065},
	{1021, 4055},
	{1013, 4045},
	{988, 4018},
	{976, 4004},
	{963, 3990},
	{956, 3979},
	{948, 3967},
	{945, 3950},
	{943, 3943},
	{940, 3936},
	{940, 3925},
	{942, 3921},
	{945, 3916},
	{950, 3908},
	{951, 3904},
	{953, 3900},
	{953, 3896},
	{953, 3891},
	{953, 3883},
	{954, 3879},
	{955, 3874},
	{953, 3865},
	{955, 3861},
	{958, 3857},
	{958, 3853},
	{958, 3849},
	{965, 3842},
	{965, 3838},
	{965, 3834},
	{970, 3827},
	{972, 3824},
	{975, 3820},
	{983, 3814},
	{985, 3811},
	{988, 3808},
	{990, 3805},
	{993, 3802},
	{998, 3797},
	{1000, 3795},
	{1003, 3792},
	{1013, 3788},
	{1020, 3787},
	{1028, 3786},
	{1040, 3782},
	{1049, 3781},
	{1058, 3779},
	{1064, 3777},
	{1070, 3775},
	{1088, 3772},
	{1099, 3771},
	{1110, 3769},
	{1130, 3765},
	{1144, 3764},
	{1158, 3762},
	{1169, 3760},
	{1180, 3757},
	{1188, 3753},
	{1210, 3751},
	{1233, 3749},
	{1265, 3744},
	{1284, 3742},
	{1303, 3739},
	{1333, 3733},
	{1327, 3731},
	{1320, 3728},
	{1310, 3724},
	{1293, 3716},
	{1285, 3713},
	{1280, 3711},
	{1265, 3706},
	{1260, 3704},
	{1258, 3701},
	{1250, 3699},
	{1245, 3696},
	{1240, 3400}  
};      

// T1 0C
R_PROFILE_STRUC r_profile_t1[] =
{
        {228, 4343},
	{190, 4328},
	{342, 4318},
	{495, 4307},
	{550, 4288},
	{552, 4280},
	{555, 4272},
	{553, 4255},
	{552, 4247},
	{550, 4239},
	{548, 4224},
	{547, 4216},
	{545, 4208},
	{543, 4193},
	{543, 4178},
	{543, 4171},
	{543, 4163},
	{545, 4148},
	{545, 4141},
	{545, 4133},
	{543, 4117},
	{545, 4110},
	{548, 4103},
	{548, 4090},
	{556, 4086},
	{565, 4082},
	{570, 4071},
	{563, 4052},
	{549, 4037},
	{535, 4021},
	{520, 3993},
	{517, 3983},
	{513, 3972},
	{508, 3956},
	{506, 3950},
	{503, 3943},
	{500, 3932},
	{500, 3928},
	{500, 3923},
	{500, 3914},
	{498, 3905},
	{497, 3901},
	{495, 3896},
	{490, 3885},
	{490, 3881},
	{490, 3876},
	{490, 3867},
	{491, 3863},
	{493, 3858},
	{495, 3850},
	{495, 3846},
	{495, 3842},
	{498, 3835},
	{505, 3829},
	{505, 3826},
	{505, 3822},
	{505, 3816},
	{507, 3814},
	{510, 3811},
	{513, 3806},
	{514, 3804},
	{515, 3801},
	{518, 3796},
	{521, 3794},
	{525, 3792},
	{530, 3788},
	{535, 3784},
	{539, 3783},
	{543, 3782},
	{545, 3778},
	{550, 3777},
	{555, 3776},
	{560, 3773},
	{564, 3772},
	{568, 3770},
	{578, 3767},
	{583, 3765},
	{588, 3763},
	{600, 3760},
	{610, 3755},
	{615, 3752},
	{620, 3748},
	{635, 3742},
	{644, 3739},
	{653, 3735},
	{673, 3728},
	{683, 3724},
	{693, 3719},
	{718, 3710},
	{733, 3707},
	{748, 3703},
	{788, 3699},
	{843, 3696},
	{879, 3694},
	{915, 3692},
	{1013, 3685},
	{1066, 3672},
	{1120, 3658},
	{980, 3591},
	{845, 3536},
	{783, 3400}     
};     
/*lenovo_sw zhangrc2 change battery param 2014-05-20 begin*/
// T2 25C
R_PROFILE_STRUC r_profile_t2[] =
{
	{180, 4339},
	{180, 4319},
	{180, 4310},
	{180, 4301},
	{180, 4284},
	{180, 4276},
	{180, 4268},
	{180, 4252},
	{180, 4235},
	{180, 4227},
	{180, 4219},
	{180, 4203},
	{180, 4196},
	{180, 4189},
	{180, 4173},
	{180, 4166},
	{180, 4158},
	{180, 4143},
	{180, 4128},
	{180, 4121},
	{180, 4113},
	{180, 4098},
	{180, 4091},
	{180, 4084},
	{180, 4075},
	{180, 4069},
	{180, 4061},
	{180, 4052},
	{180, 4025},
	{180, 4014},
	{180, 4002},
	{180, 3986},
	{180, 3976},
	{180, 3973},
	{180, 3969},
	{180, 3962},
	{180, 3958},
	{180, 3953},
	{180, 3942},
	{180, 3929},
	{180, 3922},
	{180, 3914},
	{180, 3897},
	{180, 3891},
	{180, 3884},
	{180, 3872},
	{180, 3863},
	{180, 3859},
	{180, 3854},
	{180, 3846},
	{180, 3843},
	{180, 3840},
	{180, 3833},
	{180, 3830},
	{180, 3827},
	{180, 3821},
	{180, 3815},
	{180, 3813},
	{180, 3810},
	{180, 3805},
	{180, 3803},
	{180, 3801},
	{180, 3797},
	{180, 3793},
	{180, 3791},
	{180, 3788},
	{180, 3785},
	{180, 3783},
	{180, 3781},
	{180, 3777},
	{180, 3774},
	{180, 3773},
	{180, 3771},
	{180, 3767},
	{180, 3765},
	{180, 3762},
	{180, 3757},
	{180, 3751},
	{180, 3749},
	{180, 3747},
	{180, 3742},
	{180, 3740},
	{180, 3737},
	{180, 3729},
	{180, 3725},
	{180, 3721},
	{180, 3714},
	{180, 3705},
	{180, 3700},
	{180, 3694},
	{180, 3692},
	{180, 3692},
	{180, 3691},
	{180, 3689},
	{180, 3685},
	{180, 3676},
	{180, 3666},
	{180, 3610},
	{180, 3570},
	{180, 3529},
	{180, 3400}        
}; 
// T3 50C
R_PROFILE_STRUC r_profile_t3[] =
{
	{150, 4347},
	{150, 4334},
	{150, 4320},
	{150, 4309},
	{150, 4298},
	{150, 4287},
	{150, 4276},
	{150, 4265},
	{150, 4254},
	{150, 4243},
	{150, 4232},
	{150, 4222},
	{150, 4211},
	{150, 4201},
	{150, 4190},
	{150, 4180},
	{150, 4169},
	{150, 4158},
	{150, 4147},
	{150, 4137},
	{150, 4127},
	{150, 4107},
	{150, 4097},
	{150, 4086},
	{150, 4077},
	{150, 4067},
	{150, 4058},
	{150, 4048},
	{150, 4039},
	{150, 4030},
	{150, 4020},
	{150, 4010},
	{150, 4002},
	{150, 3994},
	{150, 3986},
	{150, 3978},
	{150, 3970},
	{150, 3962},
	{150, 3954},
	{150, 3945},
	{150, 3936},
	{150, 3926},
	{150, 3914},
	{150, 3902},
	{150, 3893},
	{150, 3884},
	{150, 3877},
	{150, 3869},
	{150, 3864},
	{150, 3858},
	{150, 3853},
	{150, 3847},
	{150, 3842},
	{150, 3836},
	{150, 3832},
	{150, 3828},
	{150, 3824},
	{150, 3819},
	{150, 3811},
	{150, 3808},
	{150, 3804},
	{150, 3801},
	{150, 3797},
	{150, 3794},
	{150, 3790},
	{150, 3788},
	{150, 3785},
	{150, 3782},
	{150, 3779},
	{150, 3776},
	{150, 3772},
	{150, 3765},
	{150, 3758},
	{150, 3753},
	{150, 3747},
	{150, 3743},
	{150, 3739},
	{150, 3736},
	{150, 3732},
	{150, 3729},
	{150, 3725},
	{150, 3722},
	{150, 3719},
	{150, 3715},
	{150, 3710},
	{150, 3705},
	{150, 3699},
	{150, 3694},
	{150, 3689},
	{150, 3683},
	{150, 3676},
	{150, 3676},
	{150, 3675},
	{150, 3674},
	{150, 3672},
	{150, 3669},
	{150, 3665},
	{150, 3614},
	{150, 3565},
	{150, 3515},
	{150, 3400}        
}; 
/*lenovo_sw zhangrc2 change battery param 2014-05-20 end*/
// r-table profile for actual temperature. The size should be the same as T1, T2 and T3
R_PROFILE_STRUC r_profile_temperature[] =
{
  {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
        {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
        {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 }
};    
/*lenovo_sw zhangrc2 change battery percent and internal resistance 2014-04-28 --- end*/
// ============================================================
// function prototype
// ============================================================
int fgauge_get_saddles(void);
BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature);

#endif	//#ifndef _CUST_BATTERY_METER_TABLE_H
