/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2017, Open AI Lab
 * Author: haitao@openailab.com
 */
#include <unistd.h>

#include <iostream> 
#include <functional>
#include <algorithm>
#include <fstream>
#include <iomanip>

#include "tengine_c_api.h"
#include "common_util.hpp"
#include "image_process.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

const char * model_file="./models/sqz.onnx.model";
const char * image_file="./tests/images/cat.jpg";
const char * label_file="./models/synset_words.txt";

const float channel_mean[3]={104.007, 116.669, 122.679};
const float channel_mean0[3]={ 0 , 0, 0};

using namespace TEngine;


void LoadLabelFile(std::vector<std::string>& result, const char * fname)
{
   std::ifstream labels(fname);

   std::string line;
   while (std::getline(labels, line))
      result.push_back(line);
}

   
int main(int argc, char * argv[])
{

   const char * model_name="squeezenet";
   int img_h=224;
   int img_w=224;

   /* prepare input data */
   float  * input_data=(float*) malloc (sizeof(float) * img_h *img_w *3);

   get_input_data( image_file, input_data, img_h, img_w, channel_mean0 ,1);

   init_tengine_library();

   if(request_tengine_version("0.1")<0)
       return 1;


   if(load_model(model_name,"onnx",model_file)<0)
       return 1; 

   std::cout<<"Load model successfully\n";

   graph_t graph=create_runtime_graph("graph0",model_name,NULL);

   if(!check_graph_valid(graph))
   {
       std::cout<<"Create graph0 failed\n";
       return 1;
   }

   /* get input tensor */
   int node_idx = 0;
   int tensor_idx = 0;

   tensor_t input_tensor=get_graph_input_tensor(graph , node_idx , tensor_idx );
   
   if(!check_tensor_valid(input_tensor))
   {
       std::printf("Cannot find input tensor,node_idx: %d,tensor_idx: %d\n",node_idx,tensor_idx);
       return -1;
   }

   int dims[]={1,3,img_h,img_w};

   set_tensor_shape(input_tensor,dims,4);

   /* setup input buffer */

   if(set_tensor_buffer(input_tensor,input_data,3*img_h*img_w*4)<0)
   {
       std::printf("Set buffer for tensor failed\n");
	   return -1;
   }

   /* run the graph */
   prerun_graph(graph);

   run_graph(graph,1);



   /* get output tensor */
   tensor_t output_tensor=get_graph_output_tensor(graph, node_idx, tensor_idx);
   
   if(!check_tensor_valid(output_tensor))
   {
	   std::printf("Cannot find output tensor , node_idx: %d,tensor_idx: %d\n",node_idx,tensor_idx);
	   return -1;
   }
   
   int dim_size=get_tensor_shape(output_tensor,dims,4);

   if(dim_size<0)
   {
      printf("Get output tensor shape failed\n");
      return -1;
   }

   printf("output tensor shape: [");
    
   for(int i=0;i<dim_size;i++)
      printf("%d ",dims[i]);

   printf("]\n");


   int count=get_tensor_buffer_size(output_tensor)/4;

   float *  data=(float *)(get_tensor_buffer(output_tensor));
   float * end=data+count;
 
   std::vector<float> result(data, end);

   std::vector<int> top_N=Argmax(result,5);

   std::vector<std::string> labels;

   LoadLabelFile(labels,label_file);

   for(unsigned int i=0;i<top_N.size();i++)
   {
       int idx=top_N[i];

       std::cout<<std::fixed << std::setprecision(4)<<result[idx]<<" - \"";
       std::cout<< labels[idx]<<"\"\n";
   }
 
   postrun_graph(graph);  

   destroy_runtime_graph(graph);
   remove_model(model_name);
	
   free(input_data);

   std::cout<<"ALL TEST DONE\n";

   release_tengine_library();
   return 0;
}
