///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////Car tracking project with laser_radar_data_fusion/////////////////////////////////////////
//////////////////////////////////////////////////////////////////////Copyright 2009-10 Akihiro Takeuchi///////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////main.cpp   main function of car tracking /////////////////////////////////////////////////////////////////////


//OpenCV library
//#include "cv.h"			
//#include "cxcore.h"
//#include "highgui.h"	
#include "cv.h"
#include "highgui.h"
#include "cxcore.h"
#ifdef _DEBUG
    //DebugÉÇÅ[ÉhÇÃèÍçá
    #pragma comment(lib,"cv200d.lib") 
    #pragma comment(lib,"cxcore200d.lib") 
    #pragma comment(lib,"cvaux200d.lib") 
    #pragma comment(lib,"highgui200d.lib") 
#else
    //ReleaseÉÇÅ[ÉhÇÃèÍçá
    #pragma comment(lib,"cv200.lib") 
    #pragma comment(lib,"cxcore200.lib") 
    #pragma comment(lib,"cvaux200.lib") 
    #pragma comment(lib,"highgui200.lib") 
#endif
//C++ library
#include <stdio.h>		
#include <stdlib.h>
#include <time.h>

#include <math.h>

//ORIGINAL header files
#include "Laser_func.h"
#include "car_det_func.h"
#include "Common.h"

#include "for_use_GPU.h"
#include "switch_float.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//definiton of functions//

int main(void);	//main function (Object detection)

/////////////////////////////////////////
//data name
char ldata_name[]="2010_2_3.txt";		//laser data name 
char mov_name[] = "out.avi";		//movie name

/////////////////////////////////////////
//window name
char WIN_A[]="CAR_TRACKING";		//laser data name 
char WIN_B[]="2D-mapping";			//movie name

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FILE *resFP;

struct timeval tv_memcpy_start, tv_memcpy_end;
float time_memcpy;
struct timeval tv_kernel_start, tv_kernel_end;
float time_kernel;


int device_num;

int main(void)
{
  FILE* fp;                   //file pointer
  CvCapture *capt;            //movie file capture
  fpos_t curpos,fsize;        //file size
  int ss=0;                   //image number
  int fnum=0;                 //frame number 
  bool FLAG = true;           //file end flag
  
  //parameters
  FLOAT thresh = -0.5; //threshold score of detection (default :0.0)
  FLOAT overlap = 0.4; //threshold overlap parameter (default :0.4)
  FLOAT ratio = 1;     //resize ratio
  int TH_length = 80;  //tracking length threshold
  
  /* for measurement */
  struct timeval tv;
  struct timeval tv_1process_start, tv_1process_end;
  float one_process;
  struct timeval tv_car_detection_start, tv_car_detection_end;
  float time_car_detection;
  float other;

  /* Output file for detect result */
  resFP = fopen("detect_result.dat", "w");
  
#define TIME_MEASURE
#ifdef TIME_MEASURE
  double time_mearure;
  time_mearure = (double)cv::getTickCount();
#endif
  
#ifndef ORIGINAL
  /* initialization GPU */
  init_cuda();
#endif
  
  //get laser_save data pass (file should be in savedata folder)
  char *FPASS= get_file_pass(ldata_name);	
  
  int i;
  printf("FPASS:");
  for (i=0;*(FPASS+i) != '\0';i++){
    printf("%c",*(FPASS+i));
  }
  printf("\n");
  
  //open save file 
  if ((fp = fopen(FPASS,"rb")) == NULL) { printf("file open error!!\n"); exit(EXIT_FAILURE);}
  
  //get car-detector model
  MODEL *MO=load_model(ratio);
  
  //create lesult information
  RESULT *LR = create_result(0);
  
  //Particle filter information
  PINFO P_I = {0};
  
  //get file size and current file position
  get_f_size(fp,&curpos,&fsize);
  
  //open image-window(OpenCV)
  cvNamedWindow(WIN_A,CV_WINDOW_AUTOSIZE);	//for scan point mapping on image
  //cvNamedWindow(WIN_B,CV_WINDOW_AUTOSIZE);		//for scan point 2D mapping
  
  //get movie-file pass
  //char *MPASS= get_file_pass(mov_name);	
  ////load movie file 
  //printf("%s\n",MPASS);
  //if((capt=cvCaptureFromAVI(MPASS))==NULL){printf("movie open error!!\n"); exit(EXIT_FAILURE);}
  
  ////data skip
  //skip_data(fp,capt,1200,&fnum);
  skip_data_2(fp,1,&ss);   //Ç†ÇÈà íuÇ©ÇÁÇÃâÊëúÇå©ÇÈ(ç°ÇÕ50ñáñ⁄Ç©ÇÁÇ›ÇƒÇ¢ÇÈÅB)
  
  
  
  //load laser and movie data
  //for(int im=ss;im<2000;im++)
  for(int im=1;im<=11;im++)
  //  for(int im=1;im<=1;im++)      // for profile
    {
      //int im = 11;
      gettimeofday(&tv_1process_start, NULL);
      time_memcpy = 0;
      time_kernel = 0;

      //IplImage *IMG;	//inputed-image 
      ////load movie 
      //if((IMG=cvQueryFrame(capt))==NULL){printf("end of movie\n");break;}
      
      ////create and resize 640x480 Image (for detection and tracking)
      //IplImage *IM_D=ipl_cre_resize(IMG,640,480);
      
      // load image
      IplImage *IM_D=load_suc_image(im);
      
      
      //printf("â°í∑Ç≥%d\n",3 * IM_D -> width);    
      //printf("âÊëf%d\n",(int)(unsigned char)IM_D -> imageData[0]);
      //printf("âÊëf%d\n",(int)(unsigned char)IM_D -> imageData[2*IM_D->widthStep]);
      //printf("âÊëf%d\n",(int)(unsigned char)IM_D -> imageData[1920]);
      
      //for(int k=0;k<24;k++){printf("âÊëf%d\n",(int)(unsigned char)IM_D -> imageData[k]);}
      
      ///**HOG(é©çÏ)*/
      //FLOAT **h;
      /////**h = new int[1000];*/
      //h = (FLOAT**)malloc(2*sizeof(FLOAT*));
      //h[0] =(FLOAT*) malloc(64*sizeof(FLOAT));
      //h[1] =(FLOAT*) malloc(64*sizeof(FLOAT));
      //int a=0;
      //for(int i=0;i<64;i+=0)
      //{
      //	for(int k=0;k<24;k+=3)	
      //	{
      //		*(h[0]+i) = (int)(unsigned char)IM_D -> imageData[k+a*IM_D->widthStep];
      //		//printf("h;%d\n",*(h[0]+i));
      //		//printf("h;%d\n",(int)(unsigned char)IM_D -> imageData[k+a*IM_D->widthStep]);
      //		i++;
      //	}
      //		a++;
      //}
      //
      //FLOAT Dx,Dy,G, m[20];
      //long FLOAT l;
      //int z,z2;
      //for(z2=0;z2<20;z2++){m[z2]=0;}
      //for(int c=9;c<49;c+=8)
      //{
      //	for(int d=0;d<6;d++)
      //	{
      //	Dx = *(h[0]+c+d+1)-*(h[0]+c+d-1);
      //	Dy = *(h[0]+c+d+8)-*(h[0]+c+d-8);
      //	//printf("Dx%f\n",Dx);
      //	G= sqrt(pow(Dx,2.0)+pow(Dy,2.0));
      //	l = atan2((long FLOAT)Dy,(long FLOAT)Dx)/3.141592*180.0;
      //	//printf("äpìx%f\n",l);
      //	if(l>0)
      //	{
      //		for(z=0;z<9;z++)
      //			{
      //			if(z*20<= l && l<z*20+20)
      //				{
      //				*(h[1]+z) += G;
      //				//printf("ÉqÉXÉgÉOÉâÉÄ%f\n",*(h[1]+z));
      //				break;
      //				}
      //			}
      //	}
      //	else if(l<0)
      //	{
      //		for(z=9;z<18;z++)
      //			{
      //			if(z*20-360 <= l && l <z*20+20-360)
      //				{
      //				*(h[1]+z) += G;
      //				break;
      //				}
      //			}
      //	}
      //	}
      //}
      //for(int z=0;z<18;z++)
      //{
      //	printf("å˘îzÅ@%f\n",*(h[1]+z));
      //}
      //delete [] h;
      //////////////////////////////////////
      ///////////Car-Detection//////////////
      //////////////////////////////////////

      int D_NUMS=0;           //# of detected-object		

      //IplImage *R_I = ipl_resize(IM_D,ratio);   //trime image for detection
      IplImage *R_I = IM_D;
      FLOAT *A_SCORE = ini_ac_score(R_I); //alloc accumulated score


      gettimeofday(&tv_car_detection_start, NULL);
      RESULT *CUR=car_detection(R_I,MO,thresh,&D_NUMS,A_SCORE,overlap); //detect car-boundary boxes
      gettimeofday(&tv_car_detection_end, NULL);


      //finalization(CUR,LR,&P_I,A_SCORE,R_I,TH_length);					//calculate tracking information		

      //////////////////////////////////////
      //////////laser-radar fusion//////////
      //////////////////////////////////////

      ////get scan-point data
      //SCANDATA *Sdata=get_s_data(fp,IM_D,&curpos);
      //radar_data_fusion(Sdata,IM_D,CUR,&P_I); //draw scan point on image 

      //////////////////////////////////////
      /////////////visualization////////////
      //////////////////////////////////////

      //Scan-point visualization
      //IplImage *TDMAP=draw_sdata(Sdata,IM_D,CUR);									//visualize scan data
      //detection result visualization
      show_rects(IM_D,CUR,ratio);											//visualize car-boundary-box
      //show_vector(IM_D,TDMAP,CUR,&P_I,ratio);									//visualize velocity-vector

      cvShowImage(WIN_A,IM_D);											//show image (RESULT & scan-point)
      //cvShowImage(WIN_B,TDMAP);											//show 2D scan-point MAP

      //update result 
      //update_result(LR,CUR);												//update_result

      //save result
      /*	IplImage **cimg = (IplImage **) cvAlloc (sizeof (IplImage *) * 2);
            IplImage *SaveIm = ini_Image(IM_D->width+480,480);*/
      /*char pass[255];
		sprintf_s(pass,sizeof(pass),"save2\\%d.jpg",im);*/
      //cimg[0] = IM_D;
      //cimg[1] = TDMAP;
      //SaveIm = combine_image (2, cimg);
      //cvSaveImage(pass,SaveIm);

      gettimeofday(&tv_1process_end, NULL);

#if 1
      tvsub(&tv_car_detection_end, &tv_car_detection_start, &tv);
      time_car_detection = tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;

      tvsub(&tv_1process_end, &tv_1process_start, &tv);
      one_process = tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;

      //      printf("car_detection : %f\n", time_car_detection);
      printf("memory copy    %f\n", time_memcpy);
      printf("kernel execute %f\n", time_kernel);
      printf("1process : %f\n", one_process);
#endif

#if 1
      //#if 0 // for profile
      printf("\n****** To finish program, type \"Esc\" key *****\n");
      int IN_KEY=cvWaitKey(0);
      //      if(IN_KEY==0x1b) break;
      //      if(IN_KEY==1048603) // if 'Esc' key is typed
      if(IN_KEY=='\x1b') // if 'Esc' key is typed
        break;  
      // else
      //   sleep(3);
#endif
        
      //release data
      //Release_sdata(Sdata);						//release scan point data
      cvReleaseImage(&R_I);						//release resized image
      //cvReleaseImage(&IM_D);						//release detected image

      fnum++;
      printf("No %d\n",fnum);
	}

  //close window
  cvDestroyWindow(WIN_A);	//destroy window
  //cvDestroyWindow(WIN_B);	//destroy window

  //release car-detector-model
  free_model(MO);						

  //release detection result
  release_result(LR);					

  //close and release file information
  fclose(fp);			//close laser_file 
  //cvReleaseCapture(&capt);
    
  s_free(FPASS);		//release laser_file pass
  //s_free(MPASS);		//release movie file pass

#ifndef ORIGINAL
  /* cleaning up GPU */
  clean_cuda();
#endif


#ifdef TIME_MEASURE
  time_mearure = (double)cv::getTickCount() - time_mearure;
  printf("execution time : %fms\n", time_mearure*1000./cv::getTickFrequency());
#endif

  fprintf(resFP, "execution time : %fms\n", time_mearure*1000./cv::getTickFrequency());
  fclose(resFP);

  return 0;
}
