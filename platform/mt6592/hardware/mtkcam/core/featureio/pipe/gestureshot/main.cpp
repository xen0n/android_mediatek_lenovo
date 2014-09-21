#define SCALE_MODE 1	//	0: bilinear scale from VGA; 1: bilinear scale from previous scale; 2: blur before bilinear scale from previous scale
//#define WEBCAM_INPUT	// define WEBCAME_INPUT to enable live stream from webcam, instead of bmp files

#define _SIMPC_

#ifdef _SIMPC_
//char INPUT_DIR[512]  = "D:\\¼v¹³Database\\SD\\AE1_Test\\zoom\\cheese_VGA_new\\";
//char INPUT_DIR[512]  = "in\\";
//char OUTPUT_DIR[512] = "./out/"  ;
//char INPUT_DIR[512]  = "detection_rate\\";
//char OUTPUT_DIR[512] = "./detection_rate_out/"  ;
char INPUT_DIR[512]  = "fpr\\";
char OUTPUT_DIR[512] = "./fpr_out/"  ;


#else
#define INPUT_DIR "./in/"
#define OUTPUT_DIR "./out/"
#endif


#define DETECTION_RATE_SIM
// detection rate simulation => full search for 12 times

#ifdef WEBCAM_INPUT
#include <opencv2/opencv.hpp>
cv::VideoCapture capture;
int BMP_OUTPUT_ENABLE = 0  ;
std::vector<cv::Rect> detectedObj;
#else
int BMP_OUTPUT_ENABLE = 1  ;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _SIMPC_
#include <io.h>
#include <direct.h>
#include "faces.h"
#endif

#include "MTKGD.h"

//====== simulation only ===========//
#include "bmp_utility.h"
#include "utility_sw_rotator_y.h"
#include "SWGD_Main.h"

#define	MAX_IMAGE_NUM					(20000)

static int  g_total_image_num = 0;
static char g_image_names[MAX_IMAGE_NUM][300];
static char g_bmp_name[MAX_IMAGE_NUM][300];
static unsigned int source_img_array[GD_SCALE_NUM];

static const int image_input_width_vga = 640;
static const int image_input_height_vga = 480;
static unsigned int image_width_array[GD_SCALE_NUM] = {320, 256, 204, 160, 128, 102, 80, 64, 50, 40, 32};
static unsigned int image_height_array[GD_SCALE_NUM] = {240, 192, 152, 120, 96, 76, 60, 48, 38, 30, 24};

static unsigned int face_size_just_for_reference[GD_SCALE_NUM] = {24, 30, 37, 48, 60, 75, 96, 120, 153, 192, 240};

static unsigned char* WorkingBuffer;
static unsigned int   WorkingBufferSize = 5000000;

static BITMAP bmp_src;
static BITMAP bmp_work;

void EnumImages();
void LoadImage(int i, kal_uint16* image_buffer);
void vga2qvga_rgb(kal_uint16**, kal_uint16**);
void qvga2vga_y(kal_uint8*, kal_uint8*);

void AllocScaleImages(kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray);
void CreateScaleImages(kal_uint16* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray);
void CreateScaleImagesFromVGA_Y(kal_uint8* image_buffer_vga, kal_uint8* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray);
void CreateOnlyQVGAScaleImageFromVGA_Y(kal_uint8* image_buffer_vga, kal_uint8* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray);
void FreeScaleImages(void);
void LoadImageToRGB565(BITMAP *bmp_src, kal_uint16* image_buffer, int width, int height);
void LoadImageToY(BITMAP *bmp_src, kal_uint8* image_buffer, int width, int height);
extern int save;

int main(int argc, char **argv)
{
    FILE* fp;
    kal_char	output_fname[256];
    fp = fopen( "out//result.txt", "wb" )  ;

    MTKGestureDetector* pDetectionInterface;
    MTKGDInitInfo GDInitInfo;
    MUINT32 DetectResultSize;
    result  DetectResult[MAX_FACE_NUM];

    int i, j, bigger_size;
    unsigned char *ImageScaleBuffer, *ImageScaleBuffer_vga;
    unsigned int  ImageScaleTotalSize;
    unsigned char *TempPtr;
    unsigned char *YImage_qvga;  

    pDetectionInterface = MTKGestureDetector::createInstance(DRV_GD_OBJ_SW);

    WorkingBuffer = (unsigned char*)malloc(WorkingBufferSize);

    GDInitInfo.WorkingBufAddr = (MUINT32)WorkingBuffer;
    GDInitInfo.WorkingBufSize = WorkingBufferSize;
    GDInitInfo.GDThreadNum = 1;
    GDInitInfo.GDThreshold = 32;
    GDInitInfo.MajorFaceDecision = 1;
    GDInitInfo.SmoothLevel = 1;
    GDInitInfo.GDSkipStep = 4;
    GDInitInfo.GDRectify = 5;
    GDInitInfo.GDRefresh = 70;
    GDInitInfo.GSensor = 1;

    /*    Prepare MDP Buffer    */
    //AllocScaleImages(image_width_array, image_height_array);

	//============= prepare input image ===============//
#ifndef WEBCAM_INPUT
    EnumImages();
#endif
    ImageScaleTotalSize = 0;
    for(i=0; i<GD_SCALE_NUM;i++)
    {
        ImageScaleTotalSize += image_width_array[i]*image_height_array[i];
    }
    ImageScaleBuffer = (kal_uint8*)malloc(ImageScaleTotalSize);
    ImageScaleBuffer_vga = (kal_uint8*)malloc(image_input_width_vga * image_input_height_vga);   
    YImage_qvga = (kal_uint8*)malloc(image_width_array[0]*image_height_array[0]);

#ifndef DETECTION_RATE_SIM
	pDetectionInterface->GdInit(&GDInitInfo);
#endif

#ifdef WEBCAM_INPUT
    cv::Mat rgbVga, yVga; 
    capture.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
    capture.open(0);
    if (!capture.isOpened())
    {
        std::cout << "Error when opening camera!!!!!\n";
        return -1;
    }
    while (cv::waitKey(5) != 27) // until Esc is pressed
    {
        bool bRead = capture.read(rgbVga);
        if (!bRead)
            break;

        cv::cvtColor(rgbVga, yVga, CV_RGB2GRAY);
#else
	for ( i = 0 ; i < g_total_image_num ; i ++ ) 
	{
		bmp_read( g_image_names[i], &bmp_src )  ;
		LoadImageToY(&bmp_src, ImageScaleBuffer_vga, image_input_width_vga, image_input_height_vga);
#endif
		/*    Create scale images    */
		MTK_GD_OPERATION_MODE_ENUM mode;
		pDetectionInterface->GdGetMode(&mode);
		// for GD, must be GFD

		if( (mode & MTK_GD_GFD_MODE))
		{
#ifdef WEBCAM_INPUT
            CreateScaleImagesFromVGA_Y(yVga.data, ImageScaleBuffer, image_width_array, image_height_array);
#else
			CreateScaleImagesFromVGA_Y(ImageScaleBuffer_vga, ImageScaleBuffer, image_width_array, image_height_array);
#endif
		}	

#ifdef WEBCAM_INPUT
        detectedObj.clear();
#endif

#ifdef DETECTION_RATE_SIM
		pDetectionInterface->GdInit(&GDInitInfo);

		for(j=0;j<GDONLY_STAGE_NUM;j++)
		{
			pDetectionInterface->GdMain( (MUINT32)ImageScaleBuffer, MTK_GD_GFD_MODE, GD_GSENSOR_DIRECTION_0, 0);
			pDetectionInterface->GdGetResult( (MUINT32)DetectResult, GD_TRACKING_DISPLAY);
       
			if(DetectResult[0].face_num > 0)
			{
#ifdef WEBCAM_INPUT
                for (int ii = 0 ; ii < DetectResult[0].face_num ; ii++ )
                {
                    cv::Point pt0, pt1;
                    pt0.x = DetectResult[ii].x0 * 2;
                    pt0.y = DetectResult[ii].y0 * 2;
                    pt1.x = DetectResult[ii].x1 * 2;
                    pt1.y = DetectResult[ii].y1 * 2;
                    detectedObj.push_back(cv::Rect(pt0, pt1));
                }
#else
				break;      
#endif
			}
		}
#else
		pDetectionInterface->GdMain( (MUINT32)ImageScaleBuffer, MTK_GD_GFD_MODE, GD_GSENSOR_DIRECTION_0, 0);
		pDetectionInterface->GdGetResult( (MUINT32)DetectResult, GD_TRACKING_DISPLAY);

#ifdef WEBCAM_INPUT
        for (int ii = 0 ; ii < DetectResult[0].face_num ; ii++ )
        {
            cv::Point pt0, pt1;
            pt0.x = DetectResult[ii].x0 * 2;
            pt0.y = DetectResult[ii].y0 * 2;
            pt1.x = DetectResult[ii].x1 * 2;
            pt1.y = DetectResult[ii].y1 * 2;
            detectedObj.push_back(cv::Rect(pt0, pt1));
        }
#endif

#endif  
		pDetectionInterface->GdGetResult( (MUINT32)DetectResult, GD_TRACKING_DISPLAY);
		MtkCameraFaceMetadata fd_result;
	    MtkCameraFace faces[MAX_TRACKING_FACE_NUM];
		MtkFaceInfo   posInfo[MAX_TRACKING_FACE_NUM];
		fd_result.faces = &faces[0];
		fd_result.posInfo = &posInfo[0];

#ifdef WEBCAM_INPUT
        for (int ii = 0; ii < detectedObj.size(); ++ii)
		{
            cv::rectangle(rgbVga, detectedObj[ii], CV_RGB(255, 0, 0), 2);
        }
        cv::imshow("Webcam", rgbVga);
#else
		//pDetectionInterface->GdGetICSResult((MUINT32) &fd_result, (MUINT32) DetectResult, 320, 240, 0, 0, 0, 0);
		//================== Print result info =====================//
		printf( "[%04d] %s\n", i, g_bmp_name[ i ] )  ;
		for (j = 0 ; j < DetectResult[0].face_num ; j++ )
		{
			printf("\t%3d\t%3d\t%3d\t%3d\t%3d\n",
				DetectResult[j].x0,
				DetectResult[j].y0,
				DetectResult[j].x1,
				DetectResult[j].y1,
				DetectResult[j].rip_dir)  ;
            
			fprintf(fp, "%s\t%3d\t%3d\t%3d\t%3d\t%3d\n",g_bmp_name[i],
				DetectResult[j].x0,
				DetectResult[j].y0,
				DetectResult[j].x1,
				DetectResult[j].y1,
				DetectResult[j].rip_dir)  ;

			if ( BMP_OUTPUT_ENABLE == 1 )
			{
				int color;
				if(DetectResult[j].type == GESTUREDETECT_FACETYPE_GFD)
					color = 0x000000FF;
				else
					color = 0x0000FF00;

				// VGA size
				bmp_draw_rect( &bmp_src,
					DetectResult[j].x0<<1,
					DetectResult[j].y0<<1,
					DetectResult[j].x1<<1,
					DetectResult[j].y1<<1,
					color, 1, 0 )  ;
			}
		}		

		//====== output bmp with GD result window =========//
		if ( BMP_OUTPUT_ENABLE == 1 ) 
		{
			sprintf( output_fname, "%s//%s", OUTPUT_DIR, g_bmp_name[i] )  ;
			bmp_write( output_fname, &bmp_src )  ;
		}
		bmp_free(&bmp_src);

#endif

#ifdef DETECTION_RATE_SIM
		pDetectionInterface->GdReset();
#endif
	}      
    free(ImageScaleBuffer_vga);
    free(ImageScaleBuffer);
    free(YImage_qvga);
    free(WorkingBuffer);

#ifndef DETECTION_RATE_SIM
	pDetectionInterface->GdReset();
#endif
    pDetectionInterface->destroyInstance();
    fclose(fp);
	return 0;
}




#define SIM_IMAGE_NUM 200
BITMAP bmp_prz_original		;
BITMAP bmp_prz_resized			;
BITMAP bmp_prz_work			;

//static int		FD_SCALE_NUM	=	11	;
static int		PRZ_USEL		=	7   ;
static int		PRZ_DSEL		=	15  ;
static int		PRZ_ECV			=	0	;
static int		PRZ_EE_EN		=	0	;
static int		PRZ_EE_VSTR		=	0	;
static int		PRZ_EE_HSTR		=	0	;
static int		PRZ_CONV		=	0	;
static float	PRZ_RATIO		= (1.0f/1.25f)  ;
extern void prz( BITMAP *bmp_in, BITMAP *bmp_out, int USEL, int DSEL, int ECV, int EE_EN, int EE_VSTR, int EE_HSTR, int CONV )  ;


#ifdef _SIMPC_
void	getFirstImage(const char *path, long *hFile, struct _finddata_t *c_file);
int		getNextImage(long *hFile, struct _finddata_t *c_file);
#endif

void EnumImages()
{
#ifdef _SIMPC_
	long hFile = 0;
	struct _finddata_t c_file;

	getFirstImage( INPUT_DIR, &hFile, &c_file )  ;

	sprintf(g_image_names[g_total_image_num], INPUT_DIR);
	strcat(g_image_names[g_total_image_num], c_file.name);
	sprintf(g_bmp_name[g_total_image_num], c_file.name);
	g_total_image_num++;

	while ( getNextImage( &hFile, &c_file ) != 0 )
	{
		sprintf( g_image_names[g_total_image_num], INPUT_DIR )  ;
		strcat( g_image_names[g_total_image_num], c_file.name )  ;
		sprintf( g_bmp_name[g_total_image_num], c_file.name )  ;
		g_total_image_num ++  ;
	}
#else
    int i;
    int current_scale;
    char filenum[6] = {0};
    sprintf(filenum, "");

    for(i=0;i<SIM_IMAGE_NUM;i++)
    {
        /*
        filenum[3] = (i%10)+'0';
	    filenum[2] = ((i/10)%10)+'0';
	    filenum[1] = ((i/10/10)%10)+'0';
        filenum[0] = ((i/10/10/10)%10)+'0';
        */

        filenum[4] = (i%10)+'0';
	    filenum[3] = ((i/10)%10)+'0';
	    filenum[2] = ((i/10/10)%10)+'0';
        filenum[1] = ((i/10/10/10)%10)+'0';
        filenum[0] = ((i/10/10/10/10)%10)+'0';

        sprintf( g_image_names[i], INPUT_DIR )  ;
        //strcat(g_image_names[i], "RIPTest01_");
        //strcat(g_image_names[i], "4P_2_");
        //strcat(g_image_names[i], "VT");
        strcat(g_image_names[i], "");
		strcat( g_image_names[i], filenum )  ;
        strcat(g_image_names[i], ".bmp");

        //sprintf(g_bmp_name[i], "RIPTest01_");
        //sprintf(g_bmp_name[i], "4P_2_");
        //sprintf(g_bmp_name[i], "VT");
        sprintf(g_bmp_name[i], "A");

		strcat( g_bmp_name[i], filenum )  ;
        strcat(g_bmp_name[i], ".bmp");
		g_total_image_num = SIM_IMAGE_NUM;

        //printf("Loading %s", g_image_names);
        //printf("\n");
    }
#endif

}
void LoadImage(int i, kal_uint16* image_buffer)
{

    bmp_read( g_image_names[i], &bmp_src )  ;
    bmp_init( &bmp_work, (int)( bmp_src.width * 1.0 ), (int)( bmp_src.height * 1.0 ), 1 )  ;
    prz( &bmp_src, &bmp_work, PRZ_USEL, PRZ_DSEL, PRZ_ECV, PRZ_EE_EN, PRZ_EE_VSTR, PRZ_EE_HSTR, PRZ_CONV )  ;
    bmp_free( &bmp_src )  ;
    bmp_buffer( &bmp_work, &bmp_src )  ;
    bmp_free( &bmp_work )  ;


	bmp_toRGB565(&bmp_src, image_buffer);
}
void LoadImageToY(BITMAP *bmp_src, kal_uint8* image_buffer, int width, int height)
{

    BITMAP bmp_work;
    BITMAP bmp_temp;//prz will replace the src image data
    bmp_init( &bmp_work, (int)( width ), (int)( height), 1 );
    bmp_buffer(bmp_src, &bmp_temp);

    if(bmp_temp.width !=width ||  bmp_temp.height!=height)
    {
        prz( &bmp_temp, &bmp_work, PRZ_USEL, PRZ_DSEL, PRZ_ECV, PRZ_EE_EN, PRZ_EE_VSTR, PRZ_EE_HSTR, PRZ_CONV )  ;
        bmp_toY(&bmp_work, image_buffer);
    }
    else
        bmp_toY(bmp_src, image_buffer);

    bmp_free( &bmp_work )  ;
    bmp_free( &bmp_temp )  ;
}
void LoadImageToRGB565(BITMAP *bmp_src, kal_uint16* image_buffer, int width, int height)
{

    BITMAP bmp_work;
    BITMAP bmp_temp;//prz will replace the src image data
    bmp_init( &bmp_work, (int)( width ), (int)( height), 1 )  ;

    bmp_buffer(bmp_src, &bmp_temp);

    prz( &bmp_temp, &bmp_work, PRZ_USEL, PRZ_DSEL, PRZ_ECV, PRZ_EE_EN, PRZ_EE_VSTR, PRZ_EE_HSTR, PRZ_CONV )  ;

    bmp_toRGB565(&bmp_work, image_buffer);
    bmp_free( &bmp_work )  ;
    bmp_free( &bmp_temp )  ;

}
void vga2qvga_rgb(kal_uint16** vga_source, kal_uint16** qvga_target)
{
	bmp_init( &bmp_prz_resized, image_input_width_vga, image_input_height_vga, 1)  ;
    RGB565_toBMP( *vga_source, &bmp_prz_resized );
	
    bmp_init( &bmp_prz_work, image_width_array[0], image_height_array[0], 1 )  ;
    prz( &bmp_prz_resized, &bmp_prz_work, PRZ_USEL, PRZ_DSEL, PRZ_ECV, PRZ_EE_EN, PRZ_EE_VSTR, PRZ_EE_HSTR, PRZ_CONV )  ;
    bmp_free( &bmp_prz_resized )  ;

	bmp_toRGB565(&bmp_prz_work, *qvga_target);
	bmp_free( &bmp_prz_work );
}

void qvga2vga_y(kal_uint8* qvga_source, kal_uint8* vga_target)
{
    UTL_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    UtlRisizerInfo.srcAddr = qvga_source;
    UtlRisizerInfo.srcWidth= image_width_array[0];
    UtlRisizerInfo.srcHeight= image_height_array[0];
    UtlRisizerInfo.dstAddr = vga_target;
    UtlRisizerInfo.dstWidth = image_input_width_vga;
    UtlRisizerInfo.dstHeight = image_input_height_vga;
    UtilswBilinearResizer(&UtlRisizerInfo);
}

void AllocScaleImages(kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray)
{
    kal_int32 current_scale;
    //Generate the 11 imgae scales to source_img_array[]
    for ( current_scale = 0 ; current_scale < GD_SCALE_NUM ; current_scale ++ )
    {
        source_img_array[current_scale] = (kal_uint32)malloc(ImageWidthArray[current_scale]*ImageHeightArray[current_scale]*2);
    }
}

void CreateScaleImagesFromVGA_Y(kal_uint8* image_buffer_vga, kal_uint8* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray)
{
    kal_int32 current_scale;
    UTL_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    kal_uint8 *dst_ptr, *src_ptr;
    dst_ptr = image_buffer;

	//	original bilinear resizer
    for ( current_scale = 0 ; current_scale < GD_SCALE_NUM ; current_scale ++ )
    {
#if (SCALE_MODE == 0)
    	UtlRisizerInfo.srcAddr = image_buffer_vga;
        UtlRisizerInfo.srcWidth= image_input_width_vga;
        UtlRisizerInfo.srcHeight= image_input_height_vga;
        UtlRisizerInfo.dstAddr = dst_ptr;
        UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
        UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
        UtilswBilinearResizer(&UtlRisizerInfo);

        /*
        FILE* fp;
        char filename[20];
        sprintf(filename, "%d_%dx%d.raw", current_scale, ImageWidthArray[current_scale], ImageHeightArray[current_scale]);
        fp =fopen(filename, "wb");
        fwrite( (void*)dst_ptr, ImageWidthArray[current_scale]*ImageHeightArray[current_scale], 1, fp);
        fclose(fp);
        */
        dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];

#elif (SCALE_MODE == 1)
		if (current_scale == 0)
		{
			UtlRisizerInfo.srcAddr = image_buffer_vga;
			UtlRisizerInfo.srcWidth= image_input_width_vga;
			UtlRisizerInfo.srcHeight= image_input_height_vga;
		}
		else
		{
			UtlRisizerInfo.srcAddr = src_ptr;
			UtlRisizerInfo.srcWidth= ImageWidthArray[current_scale - 1];
			UtlRisizerInfo.srcHeight= ImageHeightArray[current_scale - 1];
		}

		UtlRisizerInfo.dstAddr = dst_ptr;
		UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
		UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
		UtilswBilinearResizer(&UtlRisizerInfo);

		src_ptr = dst_ptr;
		dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];

#elif (SCALE_MODE == 2)
		if (current_scale == 0)
		{
			UtlRisizerInfo.srcAddr = image_buffer_vga;
			UtlRisizerInfo.srcWidth= image_input_width_vga;
			UtlRisizerInfo.srcHeight= image_input_height_vga;
		}
		else
		{
			UtlRisizerInfo.srcAddr = src_ptr;
			UtlRisizerInfo.srcWidth= ImageWidthArray[current_scale - 1];
			UtlRisizerInfo.srcHeight= ImageHeightArray[current_scale - 1];
		}

		if (current_scale > 0)
		{
			cv::Mat tmp(UtlRisizerInfo.srcHeight, UtlRisizerInfo.srcWidth, CV_8UC1, UtlRisizerInfo.srcAddr, UtlRisizerInfo.srcWidth);
			//blur(tmp, tmp, cv::Size(3, 3));
			GaussianBlur(tmp, tmp, cv::Size(3, 3), GaussBlurStd, 0);
		}

		UtlRisizerInfo.dstAddr = dst_ptr;
		UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
		UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
		UtilswBilinearResizer(&UtlRisizerInfo);

		src_ptr = dst_ptr;
		dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];
#endif
    }

	////	save image
	//char buf[255] = "c:/temp/original.bmp";
	//cv::Mat img(image_input_height_vga, image_input_width_vga, CV_8UC1, image_buffer_vga, image_input_width_vga);
	//imwrite(buf, img);

	//dst_ptr = image_buffer;
	//for ( current_scale = 0 ; current_scale < GD_SCALE_NUM ; current_scale ++ )
	//{
	//	sprintf(buf, "c:/temp/%.5d.bmp", current_scale);
	//	img = cv::Mat(ImageHeightArray[current_scale], ImageWidthArray[current_scale], CV_8UC1, dst_ptr, ImageWidthArray[current_scale]);
	//	imwrite(buf, img);
	//	dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];
	//}	
}

void CreateOnlyQVGAScaleImageFromVGA_Y(kal_uint8* image_buffer_vga, kal_uint8* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray)
{
    kal_int32 current_scale;
    UTL_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    kal_uint8* dst_ptr;
    dst_ptr = image_buffer;

    for ( current_scale = 0 ; current_scale < 1 ; current_scale ++ )
    {

    	UtlRisizerInfo.srcAddr = image_buffer_vga;
        UtlRisizerInfo.srcWidth= image_input_width_vga;
        UtlRisizerInfo.srcHeight= image_input_height_vga;
        UtlRisizerInfo.dstAddr = dst_ptr;
        UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
        UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
        UtilswBilinearResizer(&UtlRisizerInfo);

        /*
        FILE* fp;
        char filename[20];
        sprintf(filename, "%d_%dx%d.raw", current_scale, ImageWidthArray[current_scale], ImageHeightArray[current_scale]);
        fp =fopen(filename, "wb");
        fwrite( (void*)dst_ptr, ImageWidthArray[current_scale]*ImageHeightArray[current_scale], 1, fp);
        fclose(fp);
        */
        dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];
    }

}

void CreateScaleImagesFromQVGA_Y(kal_uint8* image_buffer, kal_uint32* ImageWidthArray, kal_uint32* ImageHeightArray)
{
    kal_int32 current_scale;
    UTL_BILINEAR_Y_RESIZER_STRUCT UtlRisizerInfo;
    kal_uint8* dst_ptr;
    dst_ptr = image_buffer + ImageWidthArray[0]*ImageHeightArray[0];

    for ( current_scale = 1 ; current_scale < GD_SCALE_NUM ; current_scale ++ )
    {

    	UtlRisizerInfo.srcAddr = image_buffer;
        UtlRisizerInfo.srcWidth= ImageWidthArray[0];
        UtlRisizerInfo.srcHeight= ImageHeightArray[0];
        UtlRisizerInfo.dstAddr = dst_ptr;
        UtlRisizerInfo.dstWidth = ImageWidthArray[current_scale];
        UtlRisizerInfo.dstHeight = ImageHeightArray[current_scale];
        UtilswBilinearResizer(&UtlRisizerInfo);

        /*
        FILE* fp;
        char filename[20];
        sprintf(filename, "%d_%dx%d.raw", current_scale, ImageWidthArray[current_scale], ImageHeightArray[current_scale]);
        fp =fopen(filename, "wb");
        fwrite( (void*)dst_ptr, ImageWidthArray[current_scale]*ImageHeightArray[current_scale], 1, fp);
        fclose(fp);
        */
        dst_ptr+= ImageWidthArray[current_scale]*ImageHeightArray[current_scale];
    }

}

void FreeScaleImages(void)
{
    kal_int32 current_scale;
    for ( current_scale = 0 ; current_scale < GD_SCALE_NUM ; current_scale ++ ) {

        free( (void*)source_img_array[current_scale]);
    }
}
/*
 * get the file list & 1st image from the spacified path
 * Currently, the image files are defined as .bmp, .BMP, .jpg, .JPG, .jpeg, .JPEG, .png, .PNG
 *
 * Input:
 * - path: path to retrive file list
 *
 * Output:
 * - hFile: pointer to the file list
 * - c_file: pointer to the 1st file
 */
#ifdef _SIMPC_
void getFirstImage(const char *path, long *hFile, struct _finddata_t *c_file)
{
    char cur_dir[512], temp_str[512], *pch1 = NULL, *pch2 = NULL;

    //------------------------------------------------------------------------------
    // Generate image file list
    //------------------------------------------------------------------------------

    if (_getcwd(cur_dir, 512) == NULL)
    {
        fprintf(stderr, "Error!! Unable to change the directory: %s\n", path); fflush(stderr);
        exit(-1);
    }

    if (_chdir(path))
    {
        fprintf(stderr, "Error!! Unable to locate the directory: %s\n", path); fflush(stderr);
        exit(-1);
    }

    if (((*hFile) = _findfirst("*.*", c_file)) == -1L)
    {
        fprintf(stderr, "Error!! No files in specified directory!\n"); fflush(stderr);
        exit(-1);
    }

    // get file extension
    strcpy(temp_str, c_file->name);
    pch1 = strtok(temp_str, ".");
    if (pch1 != NULL)
    {
        pch2 = strtok(NULL, ".");
        while (pch2 != NULL)
        {
            pch1 = pch2;
            pch2 = strtok(NULL, ".");
        }
    }
    // ensure the file extension is the image file
    while ((pch1 == 0) || ((strcmp(pch1, "bmp") != 0) && (strcmp(pch1, "BMP") != 0) && (strcmp(pch1, "jpg") != 0) && (strcmp(pch1, "JPG") != 0) && (strcmp(pch1, "jpeg") != 0) && (strcmp(pch1, "JPEG") != 0) && (strcmp(pch1, "png") != 0) && (strcmp(pch1, "PNG") != 0)))
    {
        if (_findnext(*hFile, c_file) != 0)                                             // Find the rest of the files
        {   // No more files
            fprintf(stderr, "Error!! No image files in specified directory!\n"); fflush(stderr);
            exit(-1);
        }

        strcpy(temp_str, c_file->name);

        // get file extension
        pch1 = strtok(temp_str, ".");
        if (pch1 == NULL)
            continue;
        pch2 = strtok(NULL, ".");
        while (pch2 != NULL)
        {
            pch1 = pch2;
            pch2 = strtok(NULL, ".");
        }
    }
    _chdir(cur_dir);
}

/*
 * get the next image from the flie list
 * Currently, the image files are defined as .bmp, .BMP, .jpg, .JPG, .jpeg, .JPEG, .png, .PNG
 *
 * Input:
 * - hFile: pointer to the file list
 *
 * Output:
 * - c_file: pointer to the next image file
 *
 * Return:
 * 1: reading success, c_file now is the next image file
 * 0: fail, no more file to read
 */
int getNextImage(long *hFile, struct _finddata_t *c_file)
{
    int len = 0;
    char fn[512], *pch1 = NULL, *pch2 = NULL;

    do
    {
        if (_findnext(*hFile, c_file) != 0)                                                 // Find the next file
            return 0;                                                                       // No more files

        strcpy(fn, c_file->name);

        // get file extension
        pch1 = strtok(fn, ".");
        if (pch1 == NULL)
            continue;
        pch2 = strtok(NULL, ".");
        while (pch2 != NULL)
        {
            pch1 = pch2;
            pch2 = strtok(NULL, ".");
        }
    } while ((pch1 == 0) || ((strcmp(pch1, "bmp") != 0) && (strcmp(pch1, "BMP") != 0) && (strcmp(pch1, "jpg") != 0) && (strcmp(pch1, "JPG") != 0) && (strcmp(pch1, "jpeg") != 0) && (strcmp(pch1, "JPEG") != 0) && (strcmp(pch1, "png") != 0) && (strcmp(pch1, "PNG") != 0)));

    return 1;
}
#endif


static void removeFilenameExt(const char *fn, char *name_str)
{
	int len = 0;
	char tmp_str[256] = "", *pch1 = NULL, *pch2 = NULL;

	strcpy(tmp_str, fn);
	// get file extension
	pch1 = strtok(tmp_str, ".");
	if (pch1 != NULL)
	{
		pch2 = strtok(NULL, ".");
		if (pch2 == NULL)
			pch1 = NULL;
		while (pch2 != NULL)
		{
			pch1 = pch2;
			pch2 = strtok(NULL, ".");
		}
	}

	if (pch1 == NULL)
	{
		strcpy(name_str, fn);
		len = strlen(name_str);
		if (name_str[len - 1] == '.')
			name_str[len - 1] = '\0';
	}
	else
	{
		len = strlen(fn) - strlen(pch1) - 1;
		strncpy(name_str, fn, len);
		name_str[len] = '\0';
	}
}

