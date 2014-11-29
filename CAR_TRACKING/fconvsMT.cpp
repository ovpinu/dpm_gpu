//C++ library (thread-functions are only supported by windows)
#include <stdio.h>		
#include <stdlib.h>
//#include <windows.h>
//#include <process.h>
#include <math.h>
#include <string.h>

//Original header
#include "MODEL_info.h"		//File information
#include "Common.h"

#include <pthread.h>
#include "for_use_GPU.h"
#include "drvapi_error_string.h"
#include "multithreading.h"
#include "multithreading.cpp"

#define MY_CUDA_CHECK(res, text)                                        \
  if ((res) != CUDA_SUCCESS) {                                          \
    printf("%s failed: res = %d\n->%s\n", (text), (res), getCudaDrvErrorString((res))); \
    exit(1);                                                            \
  }

//CUdeviceptr root_C_dev, part_C_dev;
//CUdeviceptr root_error_array_dev, part_error_array_dev;
int *part_error_array;
int part_error_array_num;
size_t SUM_SIZE_C;
FLOAT *dst_C;



/* static structure to pass the multi-arguments to p-thread */
/* NOTE:A structure definition is private to a source file 
   unless placed in a shared header file. */
typedef struct {
  CUdeviceptr  C_dev;
  CUdeviceptr  A_dims_dev;
  CUdeviceptr  B_dims_dev;
  int          len;
  int          interval;
  int          L_MAX;
  CUdeviceptr  error_array_dev;
  int          error_array_num;
  int          C_dims[2];
  FLOAT       *C;
  int          calc_flag;
  CUstream     stream;
  int          max_thread_num;
  //  int          ii;
  int          level;
} fconvs_thread_arg;

/**********************************************************/
/* thread function to execute GPU kernel in each p-thread */
void *fconvs_byGPU(void *arg)
{
  /* pull out arguments from structure */
  fconvs_thread_arg *this_arg = (fconvs_thread_arg *)arg;
  CUdeviceptr C_dev           = this_arg->C_dev;
  CUdeviceptr A_dims_dev      = this_arg->A_dims_dev;
  CUdeviceptr B_dims_dev      = this_arg->B_dims_dev;
  int         len             = this_arg->len;
  int         interval        = this_arg->interval;
  int         L_MAX           = this_arg->L_MAX;
  CUdeviceptr error_array_dev = this_arg->error_array_dev;
  int         error_array_num = this_arg->error_array_num;
  // int         height          = this_arg->C_dims[0];
  // int         width           = this_arg->C_dims[1];
  int         calc_flag       = this_arg->calc_flag;
  CUstream    stream          = this_arg->stream;
  int         max_thread_num  = this_arg->max_thread_num;
  //  int         ii              = this_arg->ii;
  int         level           = this_arg->level;

  /* search maximum height and width in this level */
  int max_height = 0;
  int max_width = 0;
  for (int jj=0; jj<len; jj++)
    {
      int tmp_height = this_arg[jj].C_dims[0];
      int tmp_width  = this_arg[jj].C_dims[1];
      max_height = (max_height < tmp_height) ? tmp_height : max_height;
      max_width  = (max_width < tmp_width) ? tmp_width : max_width;
    }

  /* assign GP kernel arguments */
  void *kernel_args[] = {
    &C_dev,
    &A_dims_dev,
    &B_dims_dev,
    (void *)&len,
    (void *)&interval,
    (void *)&L_MAX,
    &error_array_dev,
    (void *)&error_array_num,
    //    (void *)&ii,
    (void *)&level
  };

  CUresult res;
  /* attach CUDA context on this pthread */
  res = cuCtxSetCurrent(ctx[0]);
  MY_CUDA_CHECK(res, "cuCtxSetCurrent(ctx[0])");

  /* define CUDA kernel shape */
  int maxThNum_perDim;
  switch(calc_flag) {
  case ROOT:
    maxThNum_perDim = sqrt(max_thread_num/len);
    break;
  case PART:
    maxThNum_perDim = sqrt(max_thread_num);
    break;
  }

#if 0
  int thread_num_x = (width < sqrt(max_thread_num)) ? width : sqrt(max_thread_num);
  int thread_num_y = (height < sqrt(max_thread_num)) ? height : sqrt(max_thread_num);

  int block_num_x = width / thread_num_x;
  int block_num_y = height / thread_num_y;
  if (width % thread_num_x != 0) block_num_x++;
  if (height % thread_num_y != 0) block_num_y++;
#else
  int thread_num_x = (max_width < maxThNum_perDim) ? max_width : maxThNum_perDim;
  int thread_num_y = (max_height < maxThNum_perDim) ? max_height : maxThNum_perDim;

  int block_num_x = max_width / thread_num_x;
  int block_num_y = max_height / thread_num_y;
  if (max_width % thread_num_x != 0) block_num_x++;
  if (max_height % thread_num_y != 0) block_num_y++;
#endif

  int sharedMemBytes = 0;

  /* launch GPU kernel */
  switch(calc_flag) {
  case ROOT:
    res = cuLaunchKernel(
                         func_process_root[0], // call function
                         block_num_x,          // gridDimX
                         block_num_y,          // gridDimY
                         1,                    // gridDimZ
                         //                         len,            // gridDimZ
                         thread_num_x,         // blockDimX
                         thread_num_y,         // blockDimY
                         //                         1,                    // blockDimZ
                         len,                    // blockDimZ
                         sharedMemBytes,       // sharedMemBytes
                         stream,               // hStream
                         kernel_args,          // kernelParams
                         NULL                  // extra
                         );
    MY_CUDA_CHECK(res, "cuLaunchKernel(root)");
    break;

  case PART:
    res = cuLaunchKernel(
                         func_process_part[0], // call function
                         block_num_x,          // gridDimX
                         block_num_y,          // gridDimY
                         //                         1,                    // gridDimZ
                         len,            // gridDimZ
                         thread_num_x,         // blockDimX
                         thread_num_y,         // blockDimY
                         1,                    // blockDimZ
                         //                         len,                    // blockDimZ
                         sharedMemBytes,       // sharedMemBytes
                         stream,               // hStream
                         kernel_args,          // kernelParams
                         NULL                  // extra
                         );
    MY_CUDA_CHECK(res, "cuLaunchKernel(part)");
    break;
  }

} /* void *fconvs_byGPU() */



/*******************************************************/
/* sub function to get texture memory reference on GPU */
void getTexRef(CUdeviceptr dev_ptr, const char* texName, CUtexref *texref_ptr, size_t memBytes)
{
  CUresult res;

  /* get handle of texture reference */
  res = cuModuleGetTexRef(texref_ptr, module[0], texName);
  MY_CUDA_CHECK(res, "cuModuleGetTexRef(texref_ptr)");

  /* bind to texture reference */
  res = cuTexRefSetAddress(NULL, *texref_ptr, dev_ptr, memBytes);
  MY_CUDA_CHECK(res, "cuTexRefSetAddress(texref_ptr)");

  /* configure texture memory */
  res = cuTexRefSetFlags(*texref_ptr, CU_TRSF_NORMALIZED_COORDINATES);
  MY_CUDA_CHECK(res, "cuTexRefSetFlags(texref)");

#ifdef USE_FLOAT_AS_DECIMAL
  res = cuTexRefSetFormat(*texref_ptr, CU_AD_FORMAT_FLOAT, 1);
  MY_CUDA_CHECK(res, "cuTexRefSetFormat(texref_ptr)");
#else
  res = cuTexRefSetFormat(*texref_ptr, CU_AD_FORMAT_UNSIGNED_INT32, 2);
  MY_CUDA_CHECK(res, "cuTexRefSetFormat(texref_ptr)");
#endif
} /* void getTexRef() */


/*******************************************************/
/* sub function to create Look-Up Table 
   to adjust pointer position in GPU */
void make_idxAdjust_LUT
(
 int *feat_size, 
 int *model_size,
 int feat_num, 
 int model_num,
 CUdeviceptr *feat_idx_LUT_dev,
 CUdeviceptr *model_idx_LUT_dev
 )
{
  CUresult res;
  int *feat_idx_LUT, *model_idx_LUT;

  /* allocate CPU memory region for LUT */
  res = cuMemHostAlloc((void **)&feat_idx_LUT, feat_num*sizeof(int), CU_MEMHOSTALLOC_PORTABLE);
  MY_CUDA_CHECK(res, "cuMemHostAlloc(feat_idx_LUT)");

  res = cuMemHostAlloc((void **)&model_idx_LUT, model_num*sizeof(int), CU_MEMHOSTALLOC_PORTABLE);
  MY_CUDA_CHECK(res, "cuMemHostAlloc(model_idx_LUT)");

  /* calculate pointer position for each level feature */
  int tmp_sum_size = 0;
  for (int level=0; level<feat_num; level++)
    {
      int height = feat_size[level*3];
      int width  = feat_size[level*3 + 1];
      int depth  = feat_size[level*3 + 2];

      feat_idx_LUT[level] = tmp_sum_size;
      tmp_sum_size += height * width * depth;
    }

  /* calculate pointer position for each model */
  tmp_sum_size = 0;
  for (int ii=0; ii<model_num; ii++)
    {
      int height = model_size[ii*3];
      int width  = model_size[ii*3 + 1];
      int depth  = model_size[ii*3 + 2];

      model_idx_LUT[ii] = tmp_sum_size;
      tmp_sum_size += height * width * depth;
    }

  /* allocate GPU memory region for LUT */
  res = cuMemAlloc(feat_idx_LUT_dev, feat_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemAlloc(feat_idx_LUT_dev)");

  res = cuMemAlloc(model_idx_LUT_dev, model_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemAlloc(model_idx_LUT_dev)");

  /* upload data to GPU */
  res = cuMemcpyHtoD(*feat_idx_LUT_dev, feat_idx_LUT, feat_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemcpyHtoD(feat_idx_LUT)");

  res = cuMemcpyHtoD(*model_idx_LUT_dev, model_idx_LUT, model_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemcpyHtoD(model_idx_LUT)");

  /* get handle to texture memory on GPU */
  CUtexref feat_idx_LUT_texref, model_idx_LUT_texref;
  res = cuModuleGetTexRef(&feat_idx_LUT_texref, module[0], "A_ptr_incrementer");
  MY_CUDA_CHECK(res, "cuModuleGetTexRef(feat_idx_LUT_texref)");

  res = cuModuleGetTexRef(&model_idx_LUT_texref, module[0], "B_ptr_incrementer");
  MY_CUDA_CHECK(res, "cuModuleGetTexRef(model_idx_LUT_texref)");

  /* bind to texture memory on GPU */
  res = cuTexRefSetAddress(NULL, feat_idx_LUT_texref, *feat_idx_LUT_dev, feat_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuTexRefSetAddress(feat_idx_LUT)");

  res = cuTexRefSetAddress(NULL, model_idx_LUT_texref, *model_idx_LUT_dev, model_num*sizeof(int));
  MY_CUDA_CHECK(res, "cuTexRefSetAddress(model_idx_LUT)");

  /* configure texture memory */
  res = cuTexRefSetFlags(feat_idx_LUT_texref, CU_TRSF_READ_AS_INTEGER);
  MY_CUDA_CHECK(res, "cuTesRefSetFlags(feat_idx_LUT_texref)");

  res = cuTexRefSetFlags(model_idx_LUT_texref, CU_TRSF_READ_AS_INTEGER);
  MY_CUDA_CHECK(res, "cuTesRefSetFlags(model_idx_LUT_texref)");

  res = cuTexRefSetFormat(feat_idx_LUT_texref, CU_AD_FORMAT_UNSIGNED_INT32, 1);
  MY_CUDA_CHECK(res, "cuTexRexSetFormat(feat_idx_LUT_texref)");

  res = cuTexRefSetFormat(model_idx_LUT_texref, CU_AD_FORMAT_UNSIGNED_INT32, 1);
  MY_CUDA_CHECK(res, "cuTexRexSetFormat(model_idx_LUT_texref)");

  /* free CPU memory region */
  res = cuMemFreeHost(feat_idx_LUT);
  MY_CUDA_CHECK(res, "cuMemFreeHost(feat_idx_LUT)");

  res = cuMemFreeHost(model_idx_LUT);
  MY_CUDA_CHECK(res, "cuMemFreeHost(model_idx_LUT)");

} /* void make_idxAdjust_LUT() */

/*******************************************************/
FLOAT ***fconvsMT_GPU
(
 CUdeviceptr featp2_dev,
 int SUM_SIZE_feat,
 FLOAT **filter,
 int *sym_info,
 int start,
 int end,
 int *A_SIZE,
 // CUdeviceptr A_SIZE_dev,
 int **B_SIZE,
 int **M_size_array,
 int L_MAX,
 int interval,
 int *FSIZE,
 int padx,
 int pady,
 int max_X,
 int max_Y,
 int calc_flag
 )
{
  CUdeviceptr root_C_dev, part_C_dev;
  CUdeviceptr root_error_array_dev, part_error_array_dev;
  start=start-1;
  end=end-1;
  
  const int len=end-start+1;
  FLOAT ***Output = (FLOAT ***)malloc(L_MAX*sizeof(FLOAT **));  // make FLOAT* Output[L_MAX][len]
  
  struct timeval tv;

  pthread_t **th_handler = (pthread_t **)calloc(L_MAX, sizeof(pthread_t *));
  pthread_t *dst_th_handler = (pthread_t *)calloc(L_MAX*len, sizeof(pthread_t));
  //  thread_data **td = (thread_data **)malloc(L_MAX*sizeof(thread_data *));  // make thread_data td[L_MAX][len] 
  fconvs_thread_arg **td = (fconvs_thread_arg **)calloc(L_MAX, sizeof(fconvs_thread_arg *));  // make fconvs_thread_arg td[L_MAX][len]
  //  thread_data *dst_td = (thread_data *)calloc(L_MAX*len, sizeof(thread_data));
  fconvs_thread_arg *dst_td = (fconvs_thread_arg *)calloc(L_MAX*len, sizeof(fconvs_thread_arg));

  unsigned long long int ptr_th_handler = (unsigned long long int)dst_th_handler;
  unsigned long long int ptr_td = (unsigned long long int)dst_td;
  for(int i=0; i<L_MAX; i++) {
    th_handler[i] = (pthread_t *)ptr_th_handler;
    //    td[i] = (thread_data *)ptr_td;
    td[i] = (fconvs_thread_arg *)ptr_td;
    ptr_th_handler += (unsigned long long int)(len*sizeof(pthread_t));
//    ptr_td += (unsigned long long int)(len*sizeof(thread_data));
    ptr_td += (unsigned long long int)(len*sizeof(fconvs_thread_arg));
  }
  

  //  int max_height=0, max_width=0;  
  
  CUresult res;
  res = cuCtxSetCurrent(ctx[0]);
  MY_CUDA_CHECK(res, "cuCtxSetCurrent(ctx[0])");

  
  int thread_num_x, thread_num_y, block_num_x, block_num_y;
  CUdeviceptr B_dims_dev;
  
  int *B_dimension = (int*)malloc(3*len*sizeof(int));
  
  CUdeviceptr B_dev;
  //  CUdeviceptr C_dev;  
  
  size_t SUM_SIZE_B = 0;
  //  size_t SUM_SIZE_C = 0;
  SUM_SIZE_C = 0;
  
  /* array in order to apply loop condition to kernel */
  int error_array_num = 0;  
  int *error_array;
  //  CUdeviceptr error_array_dev;

  /**********************************************************************/
  /* prepare output region */
  
  /* allocate output region in lump */
  FLOAT **dst_output;
  dst_output = (FLOAT **)malloc(L_MAX*len*sizeof(FLOAT *));
  if(dst_output == NULL) {
    printf("allocate dst_output failed\n");
    exit(1);
  }

  memset(dst_output, 0, L_MAX*len*sizeof(FLOAT *));  // zero clear

  /* distribution to Output[L_MAX - interval]*/
  unsigned long long int ptr_output = (unsigned long long int)dst_output;
  for(int i=0; i<L_MAX; i++) {
    Output[i] = (FLOAT **)ptr_output;
    ptr_output += (unsigned long long int)(len*sizeof(FLOAT *));
  }
  /* prepare output region */
  /**********************************************************************/
  

  /* prepare for launch kernel */
  for(int ii=0;ii<len;ii++)  // filter's loop(B's loop) 
    {
      /* store B dimendion in B_dimension */
      B_dimension[ii*3] = B_SIZE[ii][0];
      B_dimension[ii*3 + 1] = B_SIZE[ii][1];
      B_dimension[ii*3 + 2] = 31;
  
      SUM_SIZE_B += B_dimension[ii*3]*B_dimension[ii*3 + 1]*B_dimension[ii*3 + 2]*sizeof(FLOAT);
    }  //for(len)
  
  
  for(int level=interval; level<L_MAX; level++) {
    int L = level - interval;
    /**************************************************************************/
    /* loop conditon */
    //int level = ii + interval;
    if( (FSIZE[level*2]+2*pady < max_Y) || (FSIZE[level*2+1]+2*padx < max_X) ){
      error_array_num++;
      continue;
    }
    /* loop conditon */
    /**************************************************************************/
    for(int jj=0; jj<len; jj++) {
      
      /* compute size of output */
      
      int height, width;
      switch(calc_flag) {
      case ROOT:
        height = A_SIZE[level*3] - B_SIZE[jj][0] + 1;
        width = A_SIZE[level*3+1] - B_SIZE[jj][1] + 1;
        break;
      case PART:
        height = A_SIZE[L*3] - B_SIZE[jj][0] + 1;
        width = A_SIZE[L*3+1] - B_SIZE[jj][1] + 1;
        break;
      default:
        printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
        exit(1);
        break;
      }

      
      // /* search max height and max width */
      // max_height = (max_height < height) ? height : max_height;
      // max_width = (max_width < width) ? width : max_width;
      
      if (height < 1 || width < 1)
        {
          printf("Invalid input: B should be smaller than A\n");
          printf("height %d, width %d\n", height, width);  
          exit(0);
        }



      switch(calc_flag){
      case ROOT:
        //        printf("td[%d][%d] height, width = (%d, %d)\n", level, jj, height, width);
        td[level][jj].C_dims[0]=height; 
        td[level][jj].C_dims[1]=width;
        
        SUM_SIZE_C += td[level][jj].C_dims[0]*td[level][jj].C_dims[1]*sizeof(FLOAT);
        
        M_size_array[level][jj*2]=height;
        M_size_array[level][jj*2+1]=width;
        break;
        
      case PART:      
        //        printf("td[%d][%d] height, width = (%d, %d)\n", L, jj, height, width);
        td[L][jj].C_dims[0]=height; 
        td[L][jj].C_dims[1]=width;

        SUM_SIZE_C += td[L][jj].C_dims[0]*td[L][jj].C_dims[1]*sizeof(FLOAT);
        
        M_size_array[L][jj*2]=height;
        M_size_array[L][jj*2+1]=width;
        break;
        
      default:
        printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
        exit(1);
        break;
      }
    }
    //    printf("===================\n");
  }
  
  
  /* save loop condition */
  res = cuMemHostAlloc((void **)&error_array, error_array_num*sizeof(int), CU_MEMHOSTALLOC_DEVICEMAP);
  if(res != CUDA_SUCCESS) {
    printf("cuMemHostAlloc(error_array) failed: res = %s\n", conv(res));
    exit(1);
  }
  
  int hh=0;

  if (calc_flag == PART) {
    part_error_array = (int *)malloc(error_array_num*sizeof(int));
  }

  for(int level=interval; level<L_MAX; level++) {
    int L = level - interval;

    if( (FSIZE[level*2]+2*pady < max_Y) || (FSIZE[level*2+1]+2*padx < max_X) )
      { /* if this evaluation formula is TRUE, the level will not be calculated */
      
        switch(calc_flag){
        case ROOT:
          error_array[hh] = level;
          break;

        case PART:
          part_error_array[hh] = L;
          error_array[hh] = L;
          break;
          
        default:
          printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
          exit(1);
          break;
        }
        
        
        hh++;
        if(hh > error_array_num) {
          printf("beyond error_array_num!\n");
          exit(1);
        }
      }
  }
  


  /* define CUDA block shape */
  // int max_threads_num = 0;
  // //  res = cuDeviceGetAttribute(&max_threads_num, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, dev);
  // res = cuDeviceGetAttribute(&max_threads_num, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, dev[0]);
  // MY_CUDA_CHECK(res, "cuDeviceGetAttribute()");


  /* calculate max size of each block dimension */
  // switch(calc_flag) {
  // case ROOT:
  //   NR_MAXTHREADS_X = (int)sqrt((FLOAT)max_threads_num/len);
  //   NR_MAXTHREADS_Y = (int)sqrt((FLOAT)max_threads_num/len);
  //   if(NR_MAXTHREADS_X < 1) NR_MAXTHREADS_X++;
  //   if(NR_MAXTHREADS_Y < 1) NR_MAXTHREADS_Y++;
  //   break;
  // case PART:
  //    NR_MAXTHREADS_X = (int)sqrt((FLOAT)max_threads_num/len);
  //    NR_MAXTHREADS_Y = (int)sqrt((FLOAT)max_threads_num/len);
  //   // NR_MAXTHREADS_X = 2;//(int)sqrt((FLOAT)max_threads_num/len);
  //   // NR_MAXTHREADS_Y = 2;//(int)sqrt((FLOAT)max_threads_num/len);

  //   if(NR_MAXTHREADS_X < 1) NR_MAXTHREADS_X++;
  //   if(NR_MAXTHREADS_Y < 1) NR_MAXTHREADS_Y++;
  //   break;
  // default:
  //   printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
  //   exit(1);
  //   break;
  // }

  // NR_MAXTHREADS_X[0] = (int)sqrt((FLOAT)max_threads_num/len);
  // NR_MAXTHREADS_Y[0] = (int)sqrt((FLOAT)max_threads_num/len);
  // if(NR_MAXTHREADS_X[0] < 1) NR_MAXTHREADS_X++;
  // if(NR_MAXTHREADS_Y[0] < 1) NR_MAXTHREADS_Y++;
  
  // thread_num_x = (max_width < NR_MAXTHREADS_X[0]) ? max_width : NR_MAXTHREADS_X[0];
  // thread_num_y = (max_height < NR_MAXTHREADS_Y[0]) ? max_height : NR_MAXTHREADS_Y[0];
  
  // block_num_x = max_width / thread_num_x;
  // block_num_y = max_height / thread_num_y;
  // if(max_width % thread_num_x != 0) block_num_x++;
  // if(max_height % thread_num_y != 0) block_num_y++;

  /* allocate GPU memory */
  // CUdeviceptr featp2_dev;
  // res = cuMemAlloc(&featp2_dev, SUM_SIZE_feat);
  // MY_CUDA_CHECK(res, "cuMemAlloc(featp2_dev)");

  CUdeviceptr A_SIZE_dev;
  res = cuMemAlloc(&A_SIZE_dev, L_MAX*3*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemAlloc(A_SIZE_dev)");

  res = cuMemAlloc(&B_dev, SUM_SIZE_B);
  MY_CUDA_CHECK(res, "cuMemAlloc(B_dev)");

  res = cuMemAlloc(&B_dims_dev, 3*len*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemAlloc(B_dims)");

  switch(calc_flag) {
  case ROOT:
    res = cuMemAlloc(&root_error_array_dev, error_array_num*sizeof(int));
    MY_CUDA_CHECK(res, "cuMemAlloc(root_error_array_dev)");
    break;

  case PART:
    res = cuMemAlloc(&part_error_array_dev, error_array_num*sizeof(int));
    MY_CUDA_CHECK(res, "cuMemAlloc(part_array_dev)");
    part_error_array_num = error_array_num;
    break;

  default:
    printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
    exit(1);
    break;
    
  }

  /* upload data to GPU memory */
  gettimeofday(&tv_memcpy_start, NULL);
  /* upload feature */
  // res = cuMemcpyHtoD(featp2_dev, &featp2[0][0], SUM_SIZE_feat);
  // MY_CUDA_CHECK(res, "cuMemcpyHtoD(featp2_dev)");

  res = cuMemcpyHtoD(A_SIZE_dev, A_SIZE, L_MAX*3*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemcpyHtoD(A_SIZE_dev)");

  /* upload filter */
  res = cuMemcpyHtoD(B_dev, filter[start], SUM_SIZE_B);
  MY_CUDA_CHECK(res, "cuMemcpyHtoD(B_dev)");
  
  switch(calc_flag) {
  case ROOT:
    res = cuMemcpyHtoD(root_error_array_dev, error_array, error_array_num*sizeof(int));
    MY_CUDA_CHECK(res, "cuMemcpyHtoD(root_error_array_dev)");
    break;
    
  case PART:
    res = cuMemcpyHtoD(part_error_array_dev, error_array, error_array_num*sizeof(int));
    MY_CUDA_CHECK(res, "cuMemcpyHtoD(part_error_array_dev)");
    break;
    
  default:
    printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
    exit(1);
    break;
  }
  gettimeofday(&tv_memcpy_end, NULL);
  tvsub(&tv_memcpy_end, &tv_memcpy_start, &tv);
  time_memcpy += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;


  /* allocate output region on CPU memory */
  FLOAT *root_dst_C, *part_dst_C;
  switch(calc_flag) {
  case ROOT:
    res = cuMemHostAlloc((void **)&root_dst_C, SUM_SIZE_C, CU_MEMHOSTALLOC_DEVICEMAP);
    MY_CUDA_CHECK(res, "cuMemHostAlloc(root_dst_C)");
    memset(root_dst_C, 0, SUM_SIZE_C); //zero clear
    break;
  case PART:
    res = cuMemHostAlloc((void **)&part_dst_C, SUM_SIZE_C, CU_MEMHOSTALLOC_DEVICEMAP);
    MY_CUDA_CHECK(res, "cuMemHostAlloc(part_dst_C)");
    memset(part_dst_C, 0, SUM_SIZE_C); //zero clear
    dst_C = part_dst_C;                // assign global variables in order to reuse in dt_GPU.cpp
    break;
  }

  /* distribution */
  //  unsigned long long int pointer = (unsigned long long int)dst_C;
  unsigned long long int pointer;
  switch(calc_flag) {
  case ROOT:
    pointer = (unsigned long long int)root_dst_C;
    break;
  case PART:
    pointer = (unsigned long long int)part_dst_C;
    break;
  }
  for(int level=interval; level<L_MAX; level++) {
    int L = level - interval;
    for(int jj=0; jj<len; jj++) {
      switch(calc_flag) {
      case ROOT:
        td[level][jj].C = (FLOAT *)pointer;
        pointer += (unsigned long long int)(td[level][jj].C_dims[0]*td[level][jj].C_dims[1]*sizeof(FLOAT));
        break;

      case PART:
        td[L][jj].C = (FLOAT *)pointer;
        pointer += (unsigned long long int)(td[L][jj].C_dims[0]*td[L][jj].C_dims[1]*sizeof(FLOAT));
        break;

      default:
        printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
        exit(1);
        break;
      }
    }
  }

  /* allocate output region on GPU memory and upload date to GPU*/
  switch(calc_flag) {
  case ROOT:
    res = cuMemAlloc(&root_C_dev, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "cuMemAlloc(root_C_dev)");

    gettimeofday(&tv_memcpy_start, NULL);
    //    res = cuMemcpyHtoD(root_C_dev, dst_C, SUM_SIZE_C);
    res = cuMemcpyHtoD(root_C_dev, root_dst_C, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "cuMemcpyHtoD(root_C_dev)");
    gettimeofday(&tv_memcpy_end, NULL);
    tvsub(&tv_memcpy_end, &tv_memcpy_start, &tv);
    time_memcpy += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;

    break;

  case PART:
    res = cuMemAlloc(&part_C_dev, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "cuMemAlloc(part_C_dev)");

    gettimeofday(&tv_memcpy_start, NULL);
    //    res = cuMemcpyHtoD(part_C_dev, dst_C, SUM_SIZE_C);
    res = cuMemcpyHtoD(part_C_dev, part_dst_C, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "cuMemcpyHtoD(part_C_dev)");
    gettimeofday(&tv_memcpy_end, NULL);
    tvsub(&tv_memcpy_end, &tv_memcpy_start, &tv);
    time_memcpy += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;
    
    break;

  default:
    printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
    exit(1);
    break;
  }


  /* get texture reference for feature and model */
  CUtexref featp2_texref, B_texref;
#ifdef USE_FLOAT_AS_DECIMAL
  getTexRef(featp2_dev, "A", &featp2_texref, SUM_SIZE_feat);
  getTexRef(B_dev, "B", &B_texref, SUM_SIZE_B);
#else
  getTexRef(featp2_dev, "A_double", featp2_texref, SUM_SIZE_feat);
  getTexRef(B_dev, "B_double", &B_texref, SUM_SIZE_B);
#endif

  /* create Look-Up Table to adjust pointer position in GPU kernel */
  CUdeviceptr A_LUT_dev, B_LUT_dev;
  make_idxAdjust_LUT(A_SIZE, B_dimension, L_MAX, len, &A_LUT_dev, &B_LUT_dev);

  gettimeofday(&tv_memcpy_start, NULL);
  res = cuMemcpyHtoD(B_dims_dev, B_dimension, 3*len*sizeof(int));
  MY_CUDA_CHECK(res, "cuMemcpyHtoD(B_dims)");

  gettimeofday(&tv_memcpy_end, NULL);
  tvsub(&tv_memcpy_end, &tv_memcpy_start, &tv);
  time_memcpy += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;
    

  /* launch kernel
     grid shape : block_num_x * block_num_y * L_MAX, 
     block shape : thread_num_x * thread_num_y * len 
  */              
  /* dealing with 1 feature(A) by 1 z_dimension of grid */
  /* dealing with 1 model(B) by 1 z_dimension of block */

  // void *kernel_args[] = {
  //   //    &featp2_dev,                // kernel_args[0]
  //   //    &B_dev,                     // kernel_args[1]
  //   &root_C_dev,                // kernel_args[2]
  //   &A_SIZE_dev,                // kernel_args[3]
  //   &B_dims_dev,                // kernel_args[4]
  //   (void *)&len,               // kernel_args[5]
  //   (void *)&interval,          // kernel_args[6]
  //   (void *)&L_MAX,             // kernel_args[7]
  //   &root_error_array_dev,      // kernel_args[8]
  //   (void *)&error_array_num    // kernel_args[9]
  // };
  
  // if(calc_flag == PART) {
  //   // kernel_args[2] = &part_C_dev;
  //   // kernel_args[8] = &part_error_array_dev;
  //   kernel_args[0] = &part_C_dev;
  //   kernel_args[6] = &part_error_array_dev;
  // }
  // int sharedMemBytes = 0;

  gettimeofday(&tv_kernel_start, NULL);
  // switch(calc_flag) {  
  // case ROOT: 
  //   res = cuLaunchKernel(
  //                        func_process_root[0], // call function
  //                        block_num_x,       // gridDimX
  //                        block_num_y,       // gridDimY
  //                        L_MAX*len,         // gridDimZ
  //                        thread_num_x,      // blockDimX
  //                        thread_num_y,      // blockDimY
  //                        1,                 // blockDimZ
  //                        sharedMemBytes,    // sharedMemBytes
  //                        NULL,              // hStream
  //                        kernel_args,       // kernelParams
  //                        NULL               // extra
  //                        );
  //   MY_CUDA_CHECK(res, "cuLaunchKernel(root)");
  //   break;
  // case PART: 
  //   res = cuLaunchKernel(
  //                        func_process_part[0], // call function
  //                        block_num_x,       // gridDimX
  //                        block_num_y,       // gridDimY
  //                        L_MAX*len,         // gridDimZ
  //                        thread_num_x,      // blockDimX
  //                        thread_num_y,      // blockDimY
  //                        1,                 // blockDimZ
  //                        sharedMemBytes,    // sharedMemBytes
  //                        NULL,              // hStream
  //                        kernel_args,       // kernelParams
  //                        NULL               // extra
  //                        );
  //   MY_CUDA_CHECK(res, "cuLaunchKernel(part)");
  //   break;
  // default:
  //   printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
  //   exit(1);
  //   break;
  // }

  int max_thread_num = 0;
  res = cuDeviceGetAttribute(&max_thread_num, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, dev[0]);
  MY_CUDA_CHECK(res, "cuDeviceGetAttribute()");

  for (int level=interval; level<L_MAX; level++) {
    int L = level - interval;
    /**************************************************************************/
    /* loop conditon */
    if( (FSIZE[level*2]+2*pady < max_Y) || (FSIZE[level*2+1]+2*padx < max_X) ){
      continue;
    }
    /**************************************************************************/
    //    for (int jj=0; jj<len; jj++) {
  
      /* assign values to thread argument structure 
         and create GP kernel Launcher thread
      */    
      switch(calc_flag) {
      case ROOT:
#if 0
        td[level][jj].C_dev           = root_C_dev;
        td[level][jj].A_dims_dev      = A_SIZE_dev;
        td[level][jj].B_dims_dev      = B_dims_dev;
        td[level][jj].len             = len;
        td[level][jj].interval        = interval;
        td[level][jj].L_MAX           = L_MAX;
        td[level][jj].error_array_dev = root_error_array_dev;
        td[level][jj].error_array_num = error_array_num;
        td[level][jj].calc_flag       = calc_flag;
        td[level][jj].max_thread_num  = max_thread_num;
        td[level][jj].ii              = jj;
        td[level][jj].level           = level;
        res = cuStreamCreate(&(td[level][jj].stream), CU_STREAM_DEFAULT);
        MY_CUDA_CHECK(res, "cuStreamCreate(td[level][jj].stream)");
        ptohread_create(&th_handler[level][jj], NULL, fconvs_byGPU, (void *)&td[level][jj]);
#else
        td[level][0].C_dev           = root_C_dev;
        td[level][0].A_dims_dev      = A_SIZE_dev;
        td[level][0].B_dims_dev      = B_dims_dev;
        td[level][0].len             = len;
        td[level][0].interval        = interval;
        td[level][0].L_MAX           = L_MAX;
        td[level][0].error_array_dev = root_error_array_dev;
        td[level][0].error_array_num = error_array_num;
        td[level][0].calc_flag       = calc_flag;
        td[level][0].max_thread_num  = max_thread_num;
        //        td[level][0].ii              = jj;
        td[level][0].level           = level;
        res = cuStreamCreate(&(td[level][0].stream), CU_STREAM_DEFAULT);
        MY_CUDA_CHECK(res, "cuStreamCreate(td[level][0].stream)");
        pthread_create(&th_handler[level][0], NULL, fconvs_byGPU, (void *)&td[level][0]);
#endif
        break;
        
      case PART:
#if 0
        td[L][jj].C_dev           = part_C_dev;
        td[L][jj].A_dims_dev      = A_SIZE_dev;
        td[L][jj].B_dims_dev      = B_dims_dev;
        td[L][jj].len             = len;
        td[L][jj].interval        = interval;
        td[L][jj].L_MAX           = L_MAX;
        td[L][jj].error_array_dev = part_error_array_dev;
        td[L][jj].error_array_num = error_array_num;
        td[L][jj].calc_flag       = calc_flag;
        td[L][jj].max_thread_num  = max_thread_num;
        td[L][jj].ii              = jj;
        td[L][jj].level           = L;
        res = cuStreamCreate(&(td[L][jj].stream), CU_STREAM_DEFAULT);
        MY_CUDA_CHECK(res, "cuStreamCreate(td[L][jj].stream)");
        pthread_create(&th_handler[L][jj], NULL, fconvs_byGPU, (void *)&td[L][jj]);
#else
        td[L][0].C_dev           = part_C_dev;
        td[L][0].A_dims_dev      = A_SIZE_dev;
        td[L][0].B_dims_dev      = B_dims_dev;
        td[L][0].len             = len;
        td[L][0].interval        = interval;
        td[L][0].L_MAX           = L_MAX;
        td[L][0].error_array_dev = part_error_array_dev;
        td[L][0].error_array_num = error_array_num;
        td[L][0].calc_flag       = calc_flag;
        td[L][0].max_thread_num  = max_thread_num;
        //        td[L][0].ii              = jj;
        td[L][0].level           = L;
        res = cuStreamCreate(&(td[L][0].stream), CU_STREAM_DEFAULT);
        MY_CUDA_CHECK(res, "cuStreamCreate(td[L][0].stream)");
        pthread_create(&th_handler[L][0], NULL, fconvs_byGPU, (void *)&td[L][0]);
#endif
        break;
      }
      //  }
  }
  
  
  /* synchronize GPU threads */
  // res = cuCtxSynchronize();
  // MY_CUDA_CHECK(res, "cuCtxSynchronize(process)");

  /* wait for all pthread complete its work */
  for (int level=interval; level<L_MAX; level++) {
    int L = level - interval;
    /**************************************************************************/
    /* loop conditon */
    if( (FSIZE[level*2]+2*pady < max_Y) || (FSIZE[level*2+1]+2*padx < max_X) ){
      continue;
    }
    /**************************************************************************/
    //    for (int jj=0; jj<len; jj++) {
      switch(calc_flag) {
      case ROOT:
#if 0
        res = cuStreamSynchronize(td[level][jj].stream);
        MY_CUDA_CHECK(res, "cuStreamSynchronize(td[level][jj].stream)");

        pthread_join(th_handler[level][jj], NULL);

        res = cuStreamDestroy(td[level][jj].stream);
        MY_CUDA_CHECK(res, "cuStreamDestroy(td[level][jj].stream)");
#else
        res = cuStreamSynchronize(td[level][0].stream);
        MY_CUDA_CHECK(res, "cuStreamSynchronize(td[level][0].stream)");

        pthread_join(th_handler[level][0], NULL);

        res = cuStreamDestroy(td[level][0].stream);
        MY_CUDA_CHECK(res, "cuStreamDestroy(td[level][0].stream)");
#endif
        break;

      case PART:
#if 0
        res = cuStreamSynchronize(td[L][jj].stream);
        MY_CUDA_CHECK(res, "cuStreamSynchronize(td[L][jj].stream)");

        pthread_join(th_handler[L][jj], NULL);

        res = cuStreamDestroy(td[L][jj].stream);
        MY_CUDA_CHECK(res, "cuStreamDestroy(td[L][jj].stream)");
#else
        res = cuStreamSynchronize(td[L][0].stream);
        MY_CUDA_CHECK(res, "cuStreamSynchronize(td[L][0].stream)");

        pthread_join(th_handler[L][0], NULL);

        res = cuStreamDestroy(td[L][0].stream);
        MY_CUDA_CHECK(res, "cuStreamDestroy(td[L][0].stream)");
#endif
        break;
      }
      //    }
  }


  gettimeofday(&tv_kernel_end, NULL);
  tvsub(&tv_kernel_end, &tv_kernel_start, &tv);
  time_kernel += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;


  /* download C from GPU */
  gettimeofday(&tv_memcpy_start, NULL);
  switch(calc_flag) { 
  case ROOT: 
    // res = cuMemcpyDtoH((void *)dst_C, root_C_dev, SUM_SIZE_C);
    // MY_CUDA_CHECK(res, "bcuMemcpyDtoH(dst_C_root)");
    res = cuMemcpyDtoH((void *)root_dst_C, root_C_dev, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "bcuMemcpyDtoH(root_dst_C)");
    break;

  case PART:
    // res = cuMemcpyDtoH((void *)dst_C, part_C_dev, SUM_SIZE_C);
    // MY_CUDA_CHECK(res, "cuMemcpyDtoH(dst_C_part)");
    res = cuMemcpyDtoH((void *)part_dst_C, part_C_dev, SUM_SIZE_C);
    MY_CUDA_CHECK(res, "cuMemcpyDtoH(part_dst_C)");
    break;

  default:
    printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
    exit(1);
    break;
    
  }
  gettimeofday(&tv_memcpy_end, NULL);
  tvsub(&tv_memcpy_end, &tv_memcpy_start, &tv);
  time_memcpy += tv.tv_sec * 1000.0 + (float)tv.tv_usec / 1000.0;
  

  /* close handle and get output */
  for(int level=interval; level<L_MAX; level++) {
    int L = level - interval;
    /**************************************************************************/
    /* loop condition */
    if( (FSIZE[level*2]+2*pady < max_Y) || (FSIZE[level*2+1]+2*padx < max_X) )
      {
        continue;
      }
    /* loop conditon */
    /**************************************************************************/
    for(int jj=0; jj<len; jj++) {
      switch(calc_flag){
        
      case ROOT:
        Output[level][jj] = td[level][jj].C;
        break;
        
      case PART:
        Output[L][jj] = td[L][jj].C;
        break;
        
      default:
        printf("NOT DEFINED value: calc_flag = %d\n", calc_flag);
        exit(1);
        break;
      }

    }
  }
  

  
  /* free GPU memory */
  // res = cuMemFree(featp2_dev);
  // MY_CUDA_CHECK(res, "cuMemFree(featp2_dev)");

  res = cuMemFree(A_SIZE_dev);
  MY_CUDA_CHECK(res, "cuMemFree(A_SIZE_dev)");

  res = cuMemFree(B_dev);
  MY_CUDA_CHECK(res, "cuMemFree(B_dev)");

  res = cuMemFree(B_dims_dev);
  MY_CUDA_CHECK(res, "cuMemFree(B_dims_dev)");

  switch(calc_flag) {
  case ROOT:
    res = cuMemFree(root_error_array_dev);
    MY_CUDA_CHECK(res, "cuMemFree(root_error_array_dev)");

    res = cuMemFree(root_C_dev);
    MY_CUDA_CHECK(res, "cuMemFree(root_C_dev)");
    break;

  case PART:
    res = cuMemFree(part_error_array_dev);
    MY_CUDA_CHECK(res, "cuMemFree(part_error_array_dev)");

    res = cuMemFree(part_C_dev);
    MY_CUDA_CHECK(res, "cuMemFree(part_C_dev)");
    break;
  }
  
  res = cuMemFree(A_LUT_dev);
  MY_CUDA_CHECK(res, "cuMemFree(A_LUT_dev)");

  res = cuMemFree(B_LUT_dev);
  MY_CUDA_CHECK(res, "cuMemFree(B_LUT_dev)");


  
  /* free CPU memory */
  res = cuMemFreeHost((void *)error_array);
  MY_CUDA_CHECK(res, "cuMemFreeHost(error_array)");;

  
  s_free(B_dimension);
  s_free(th_handler[0]);
  s_free(th_handler);
  s_free(td[0]);	  
  s_free(td);	


  return(Output);
  
} /* FLOAT ***fconvsMT_GPU() */

