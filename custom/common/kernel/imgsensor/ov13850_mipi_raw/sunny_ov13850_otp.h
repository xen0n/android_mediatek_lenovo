/*file discription : otp*/

#define OTP_DRV_LSC_SIZE 62

struct otp_struct {
int module_integrator_id;
int lens_id;
int production_year;
int production_month;
int production_day;
int rg_ratio;
int bg_ratio;
int light_rg;
int light_bg;
int typical_rg_ratio;
int typical_bg_ratio;
char lenc[OTP_DRV_LSC_SIZE ];
int VCM_start;
int VCM_end;
int VCM_dir;
};



extern int read_otp_info(int index, struct otp_struct *otp_ptr);
extern int update_otp_wb(void);
extern int update_otp_lenc(void);







