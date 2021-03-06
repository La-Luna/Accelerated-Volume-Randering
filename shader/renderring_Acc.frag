#version 430
in vec3 EntryPoint;
/*layout(binding=0,r8ui)uniform uimage3D distance_map;
//layout(binding=0,r8ui)uniform sampler3D distance_map;
layout(binding=1)uniform sampler3D VolumeTex;
layout(binding=2,rgba8)uniform sampler1D TransferFunc;  
layout(binding=3)uniform sampler2D ExitPoints;*/
layout(location=0)out vec4 FragColor;
uniform sampler2D ExitPoints;
uniform sampler3D VolumeTex;
uniform sampler1D TransferFunc;
uniform sampler3D distance_map;/////////////////////////////////////!

uniform float     StepSize;
uniform vec2      ScreenSize;
//out vec4 FragColor;

void main()
{
    float sampling_factor=4.0f;

	vec3 exitPoint = texture(ExitPoints, gl_FragCoord.st/ScreenSize).xyz;
	//vec3 exitPoint1=normalize(exitPoint);//??
	//vec3 EntryPoint1=normalize(EntryPoint);
	    if (EntryPoint == exitPoint)
    	discard;
	vec3 ray_dir=exitPoint-EntryPoint;
	float ray_distance = distance(EntryPoint, exitPoint);

    vec4 colorAcum = vec4(0.0); // The dest color
    float alphaAcum = 0.0;                // The  dest alpha for blending
    /* 定义颜色查找的坐标 */
   // float intensity;
    vec4 colorSample; // The src color 
    float alphaSample; // The src alpha

    // Determine number of samples
    //ivec3 dim = imageSize(VolumeTex);
	  ivec3 dim = ivec3(256,256,225);
   //ivec3 dim = ivec3(64,64,56);
    int dim_max = max(max(dim.x, dim.y), dim.z);
    int n_steps = int(ceil(float(dim_max) * ray_distance * sampling_factor));////////？？？？？
    vec3 step_volume = ray_dir * ray_distance / (float(n_steps) - 1.0f);
    float sampling_factor_inv = 1.0f / sampling_factor;
  // This test fixes a performance regression if view is oriented with edge/s of the volume
  // perhaps due to precision issues with the bounding box intersection
    vec3 early_exit_test = EntryPoint + step_volume;
    if (any(lessThanEqual(early_exit_test, vec3(0))) || any(greaterThanEqual(early_exit_test, vec3(1)))) 
    {
    return;
    }
   // Empty space skipping
   ivec3 dim_distance_map = ivec3(64,64,56);
    //ivec3 dim_distance_map = imageSize(distance_map);
    ivec3 block_size = dim / dim_distance_map;  // NOTE: scalar in paper, but can be a vector
    vec3 volume_to_distance_map = vec3(dim) / (vec3(block_size) * vec3(dim_distance_map));
    vec3 step_dist_texel = step_volume * vec3(dim) / vec3(block_size);
    vec3 step_dist_texel_inv = 1.0f / step_dist_texel;
    bool skipping = true;
    int i_last_alpha = 0; // current furthest "occupied" step
    int i_resume = int(ceil(1.5f * sampling_factor * max(max(block_size.x, block_size.y), block_size.z)));

    float ray_penetration = 1.0f; // assume ray goes through

    float len = length(ray_dir); // the length from front to back is calculated and used to terminate the ray
    vec3 deltaDir = normalize(ray_dir) * StepSize;
    float deltaDirLen = length(deltaDir);
    float lengthAcum = 0.0;
  vec3 pos = EntryPoint;
      vec4 bgColor = vec4(1.0, 1.0, 1.0, 0.0);
   float intensity;

  for (int i = 0; i < n_steps;) 
  //for (int i = 0; i < 1600;) 
  {
      vec3 pos = EntryPoint + float(i) * step_volume;
	 if (skipping) {
      vec3 pos_distance_map = volume_to_distance_map * pos;
      vec3 u = pos_distance_map * vec3(dim_distance_map);
      //ivec3 u_i = ivec3(floor(u));
      ivec3 u_i =ivec3(clamp(floor(u), ivec3(0), dim_distance_map - 1));
      float dist = texelFetch(distance_map, u_i,0).x;/////////////////////////////////////!
            //uint dist = 0u;///????
      vec3 r = -fract(u);
  
	  if (dist>0u) 
      {
		    // Skip with "chebyshev empty space skipping"
      //float dist1=float(dist);/////////////////////////////////////!
      vec3 n_iter = (step(0.0f, -step_dist_texel_inv) + sign(step_dist_texel_inv) * dist + r) * step_dist_texel_inv;/////////////////////////////////////!
		  int skips = max(1, int(ceil(min(min(n_iter.x, n_iter.y), n_iter.z))));
      i += skips;
        //i+=1；
      } 
      else {
	   //colorAcum = vec4(vec3(ray_distance * float(i) / float(n_steps)), 1.0f);
	    // Stop skipping and step back
        skipping = false;
        int i_backwards = max(i - int(ceil(sampling_factor)), i_last_alpha);
        i_last_alpha = i + 1;
        i = i_backwards;
        //i+=skips;
        //i+=40;
        // NOTE: The ray is stepped backwards as sample positions just outside of occupied blocks may have some opacity (due to linear sampling of the volume)
        // The artefacts are quite subtle, so this could be optional. For correctness, this is enabled, but obviously causes a slight performance drop.
        // The ray won't ever step back further than the last sampled voxel or make the same back step twice, say if i_resume is abnormally tiny relative to the region size
      }
    }
	else{
    //ivec3 pos2=ivec3(pos.x*dim.x,pos.y*dim.y,pos.z*dim.z);
//	ivec3 pos2=ivec3(pos.x*dim_distance_map.x,pos.y*dim_distance_map.y,pos.z*dim_distance_map.z);///////////////？？
    // 获得体数据中的标量值scaler value
     intensity =texture(VolumeTex,pos).x;///////////////////////////////////////////////////！
       // uint intensity = texelFetch(VolumeTex, pos2,0).x;
    // 查找传输函数中映射后的值
    // 依赖性纹理读取  
	//float intensity2=float(intensity);
    colorSample = texture(TransferFunc,intensity);
    //colorSample=vec4(0,0,0,0);
    //    colorSample = texture(TransferFunc, 0.5);
    // modulate the value of colorSample.a
    // front-to-back integration
    if (colorSample.a > 0.0) {
    	    // accomodate for variable sampling rates (base interval defined by mod_compositing.frag)
    	    colorSample.a = 1.0 - pow(1.0 - colorSample.a, StepSize*200.0f);
    	    colorAcum.rgb += (1.0 - colorAcum.a) * colorSample.rgb * colorSample.a;
    	    colorAcum.a += (1.0 - colorAcum.a) * colorSample.a;
              if (colorAcum.a > 0.99f) {
            // Set ray penetration
            //ray_penetration = float(i) / (float(n_steps) - 1.0f);
             FragColor = colorAcum;
            return;
          }
    	}
    	//pos += deltaDir;
    	lengthAcum += deltaDirLen;
    	if (lengthAcum >= len )
    	{	
    	    colorAcum.rgb = colorAcum.rgb*colorAcum.a + (1 - colorAcum.a)*bgColor.rgb;		
    	    break;  // terminate if opacity > 1 or the ray is outside the volume	
    	}
        else if (i >= (i_last_alpha + i_resume)) {
          // Resume empty space skipping
          skipping = true;
          i_last_alpha = i + 1; // skipping won't jump back further than this if next block is occupied
        }

      // Move the ray forward
      ++i;


 }
  }
 
     //intensity =texture(VolumeTex,vec3(0.5,0.5,0.5)).x;
    //FragColor =  texture(TransferFunc,intensity);;
    FragColor = colorAcum;

}
