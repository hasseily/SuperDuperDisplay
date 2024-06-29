#ifdef GL_ES
#define COMPAT_LOWP lowp
#define COMPAT_MEDP mediump
#define COMPAT_HIGHP highp
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_LOWP
#define COMPAT_MEDP
#define COMPAT_HIGHP
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

uniform COMPAT_HIGHP int ticks;
uniform COMPAT_HIGHP int hborder;
uniform COMPAT_HIGHP usampler2D VRAMTEX;
uniform COMPAT_LOWP sampler2D a2ModesTex0;
uniform COMPAT_LOWP sampler2D a2ModesTex1;
uniform COMPAT_LOWP sampler2D a2ModesTex2;
uniform COMPAT_LOWP sampler2D a2ModesTex3;
uniform COMPAT_LOWP sampler2D a2ModesTex4;
uniform COMPAT_HIGHP int specialModesMask;
uniform COMPAT_HIGHP int monitorColorType;
in COMPAT_MEDP vec2 vFragPos;
out COMPAT_MEDP vec4 fragColor;
void main()
{
  bool tmpvar_1;
  tmpvar_1 = bool(0);
  COMPAT_HIGHP uvec2 fragOffset_2;
  uvec2 tmpvar_3;
  tmpvar_3 = uvec2(vFragPos);
  ivec2 tmpvar_4;
  tmpvar_4.x = int(uint(int((float(tmpvar_3.x) * 0.07142857))));
  tmpvar_4.y = int(uint(int((float(tmpvar_3.y) * 0.5))));
  uvec4 tmpvar_5;
  tmpvar_5 = texelFetch (VRAMTEX, tmpvar_4, 0);
  uvec2 tmpvar_6;
  tmpvar_6.x = (uint(mod (tmpvar_3.x, 14u)));
  tmpvar_6.y = (uint(mod (tmpvar_3.y, 16u)));
  fragOffset_2 = tmpvar_6;
  uint tmpvar_7;
  tmpvar_7 = (tmpvar_5.z & 7u);
  bool tmpvar_8;
  tmpvar_8 = bool(0);
  bool tmpvar_9;
  tmpvar_9 = bool(0);
  while (true)
{
    bool tmpvar_10;
    tmpvar_10 = bool(1);
    tmpvar_8 = (tmpvar_8 || (uint(0)  == tmpvar_7));
    tmpvar_8 = (tmpvar_8 || (1u == tmpvar_7));
    if (tmpvar_8)
    {
      COMPAT_MEDP vec4 tex_11;
      uint tmpvar_12;
      tmpvar_12 = (((1u + -(tmpvar_7)) * tmpvar_5.x) + (tmpvar_7 * ((tmpvar_5.x * uint(int((
        float(fragOffset_2.x)
       * 0.1428571)))) + (tmpvar_5.y * (1u + -(uint(
        int((float(fragOffset_2.x) * 0.1428571))
      )))))));
      float tmpvar_13;
      tmpvar_13 = float(tmpvar_12);
      uint tmpvar_14;
      tmpvar_14 = ((tmpvar_5.z >> 3) & 1u);
      float tmpvar_15;
      tmpvar_15 = (((1.0 + -(float((tmpvar_13 >= 128.0)))) * (1.0 + -((1.0 + -(float(
        (tmpvar_13 >= 64.0)
      )))))) * (1.0 + -(float(tmpvar_14))));
      uvec2 tmpvar_16;
      tmpvar_16.x = (tmpvar_12 & 15u);
      tmpvar_16.y = (tmpvar_12 >> 4);
      uvec2 tmpvar_17;
      tmpvar_17 = (tmpvar_16 * uvec2(14u, 16u));
      fragOffset_2.x = (((1u + -(tmpvar_7)) * fragOffset_2.x) + ((tmpvar_7 * (((fragOffset_2.x + 4294967289u) * uint(int(
        (float(fragOffset_2.x) * 0.1428571)
      ))) + (fragOffset_2.x * (1u + -(
        uint(int((float(fragOffset_2.x) * 0.1428571)))
      ))))) * 2u));
      if (bool(int(tmpvar_14)))
      {
        tex_11 = texture (a2ModesTex1, (vec2(tmpvar_17 + fragOffset_2) + vec2(0.5, 0.5)) * (1.0/(vec2(textureSize (a2ModesTex1,0)))) );
      }
      else
      {
        tex_11 = texture (a2ModesTex0, (vec2(tmpvar_17 + fragOffset_2) + vec2(0.5, 0.5)) * (1.0/(vec2(textureSize (a2ModesTex0,0)))) );
      }
float tmpvar_18;
      tmpvar_18 = (tmpvar_15 * float((int(mod (int((float(ticks) * 0.003225806)), 2)))));
      tex_11 = (((1.0 + -(tex_11)) * tmpvar_18) + (tex_11 * (1.0 + -(tmpvar_18))));
      if ((0 < monitorColorType))
      {
        float tmpvar_19;
        tmpvar_19 = sqrt(abs(dot (tex_11.xyz, tex_11.xyz)));
        if ((0.0 < tmpvar_19))
        {
          vec4 tmpvar_20;
          if ((monitorColorType < 2))
          {
            tmpvar_20 = vec4(0.0, 0.0, 0.0, 1.0);
            if (monitorColorType == 1) tmpvar_20 = vec4(1.0, 1.0, 1.0, 1.0);
          }
          else
          {
            tmpvar_20 = vec4(0.290196, 1.0, 0.0, 1.0);
            bvec2 tmpvar_21;
            tmpvar_21 = lessThanEqual (ivec2(monitorColorType), ivec2(3, 4));
            if (tmpvar_21.x) tmpvar_20 = vec4(1.0, 0.717647, 0.0, 1.0);
            if (tmpvar_21.y) tmpvar_20 = vec4(1.0, 0.0, 0.5, 1.0);
          }
fragColor = tmpvar_20;
        }
        else
        {
          fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
}
else
{
        vec4 tmpvar_22;
        uint tmpvar_23;
        tmpvar_23 = ((tmpvar_5.w & 240u) >> 4);
        if ((tmpvar_23 < 8u))
        {
          if ((tmpvar_23 < 4u))
          {
            tmpvar_22 = vec4(0.0, 0.0, 0.0, 1.0);
            bvec3 tmpvar_24;
            tmpvar_24 = lessThanEqual (uvec3(tmpvar_23), uvec3(1u, 2u, 3u));
            if (tmpvar_24.x) tmpvar_22 = vec4(0.67451, 0.070588, 0.298039, 1.0);
            if (tmpvar_24.y) tmpvar_22 = vec4(0.0, 0.027451, 0.513725, 1.0);
            if (tmpvar_24.z) tmpvar_22 = vec4(0.666667, 0.101961, 0.819608, 1.0);
          }
          else
          {
            tmpvar_22 = vec4(0.0, 0.513725, 0.184314, 1.0);
            bvec3 tmpvar_25;
            tmpvar_25 = lessThanEqual (uvec3(tmpvar_23), uvec3(5u, 6u, 7u));
            if (tmpvar_25.x) tmpvar_22 = vec4(0.623529, 0.592157, 0.494118, 1.0);
            if (tmpvar_25.y) tmpvar_22 = vec4(0.0, 0.541176, 0.709804, 1.0);
            if (tmpvar_25.z) tmpvar_22 = vec4(0.623529, 0.619608, 1.0, 1.0);
          }
}
else
{
          if ((tmpvar_23 < 12u))
          {
            tmpvar_22 = vec4(0.478431, 0.372549, 0.0, 1.0);
            bvec3 tmpvar_26;
            tmpvar_26 = lessThanEqual (uvec3(tmpvar_23), uvec3(9u, 10u, 11u));
            if (tmpvar_26.x) tmpvar_22 = vec4(1.0, 0.447059, 0.278431, 1.0);
            if (tmpvar_26.y) tmpvar_22 = vec4(0.470588, 0.407843, 0.498039, 1.0);
            if (tmpvar_26.z) tmpvar_22 = vec4(1.0, 0.478431, 0.811765, 1.0);
          }
          else
          {
            tmpvar_22 = vec4(0.435294, 0.901961, 0.172549, 1.0);
            bvec3 tmpvar_27;
            tmpvar_27 = lessThanEqual (uvec3(tmpvar_23), uvec3(13u, 14u, 15u));
            if (tmpvar_27.x) tmpvar_22 = vec4(1.0, 0.964706, 0.482353, 1.0);
            if (tmpvar_27.y) tmpvar_22 = vec4(0.423529, 0.933333, 0.698039, 1.0);
            if (tmpvar_27.z) tmpvar_22 = vec4(1.0, 1.0, 1.0, 1.0);
          }
}
vec4 tmpvar_28;
        uint tmpvar_29;
        tmpvar_29 = (tmpvar_5.w & 15u);
        if ((tmpvar_29 < 8u))
        {
          if ((tmpvar_29 < 4u))
          {
            tmpvar_28 = vec4(0.0, 0.0, 0.0, 1.0);
            bvec3 tmpvar_30;
            tmpvar_30 = lessThanEqual (uvec3(tmpvar_29), uvec3(1u, 2u, 3u));
            if (tmpvar_30.x) tmpvar_28 = vec4(0.67451, 0.070588, 0.298039, 1.0);
            if (tmpvar_30.y) tmpvar_28 = vec4(0.0, 0.027451, 0.513725, 1.0);
            if (tmpvar_30.z) tmpvar_28 = vec4(0.666667, 0.101961, 0.819608, 1.0);
          }
          else
          {
            tmpvar_28 = vec4(0.0, 0.513725, 0.184314, 1.0);
            bvec3 tmpvar_31;
            tmpvar_31 = lessThanEqual (uvec3(tmpvar_29), uvec3(5u, 6u, 7u));
            if (tmpvar_31.x) tmpvar_28 = vec4(0.623529, 0.592157, 0.494118, 1.0);
            if (tmpvar_31.y) tmpvar_28 = vec4(0.0, 0.541176, 0.709804, 1.0);
            if (tmpvar_31.z) tmpvar_28 = vec4(0.623529, 0.619608, 1.0, 1.0);
          }
}
else
{
          if ((tmpvar_29 < 12u))
          {
            tmpvar_28 = vec4(0.478431, 0.372549, 0.0, 1.0);
            bvec3 tmpvar_32;
            tmpvar_32 = lessThanEqual (uvec3(tmpvar_29), uvec3(9u, 10u, 11u));
            if (tmpvar_32.x) tmpvar_28 = vec4(1.0, 0.447059, 0.278431, 1.0);
            if (tmpvar_32.y) tmpvar_28 = vec4(0.470588, 0.407843, 0.498039, 1.0);
            if (tmpvar_32.z) tmpvar_28 = vec4(1.0, 0.478431, 0.811765, 1.0);
          }
          else
          {
            tmpvar_28 = vec4(0.435294, 0.901961, 0.172549, 1.0);
            bvec3 tmpvar_33;
            tmpvar_33 = lessThanEqual (uvec3(tmpvar_29), uvec3(13u, 14u, 15u));
            if (tmpvar_33.x) tmpvar_28 = vec4(1.0, 0.964706, 0.482353, 1.0);
            if (tmpvar_33.y) tmpvar_28 = vec4(0.423529, 0.933333, 0.698039, 1.0);
            if (tmpvar_33.z) tmpvar_28 = vec4(1.0, 1.0, 1.0, 1.0);
          }
}
fragColor = ((tex_11 * tmpvar_22) + ((1.0 + -(tex_11)) * tmpvar_28));
      }
tmpvar_1 = bool(1);
      tmpvar_9 = bool(1);
      tmpvar_10 = bool(0);
    }
    else
    {
      tmpvar_8 = (tmpvar_8 || (2u == tmpvar_7));
      tmpvar_8 = (tmpvar_8 || (3u == tmpvar_7));
      if (tmpvar_8)
      {
        COMPAT_HIGHP uint tmpvar_34;
        tmpvar_34 = (tmpvar_5.y >> 4);
        COMPAT_HIGHP uint tmpvar_35;
        tmpvar_35 = (tmpvar_5.y & 15u);
        uint tmpvar_36;
        tmpvar_36 = (((1u + -((tmpvar_7 + 4294967294u))) * tmpvar_5.x) + ((tmpvar_7 + 4294967294u) * ((tmpvar_5.x * uint(int((
          float(fragOffset_2.x)
         * 0.1428571)))) + ((((
          ((tmpvar_34 << 1) & 15u)
         | 
          ((tmpvar_34 >> 3) & 1u)
        ) << 4) | ((
          (tmpvar_35 << 1)
         & 15u) | (
          (tmpvar_35 >> 3)
         & 1u))) * (1u + -(uint(
          int((float(fragOffset_2.x) * 0.1428571))
        )))))));
        uvec2 tmpvar_37;
        tmpvar_37.x = uint(0);
        tmpvar_37.y = ((tmpvar_36 & 15u) * 16u);
        uvec2 tmpvar_38;
        tmpvar_38.x = uint(0);
        tmpvar_38.y = ((tmpvar_36 >> 4) * 16u);
        uvec2 tmpvar_39;
        tmpvar_39.y = 1u;
        tmpvar_39.x = (tmpvar_7 + 4294967295u);
        vec4 tmpvar_40;
        tmpvar_40 = texture (a2ModesTex2, 
	        (
		        (
			        vec2(
				        ((1u - uint(float(tmpvar_6.y) * 0.125))* tmpvar_37)
							   
				        + (uint(float(tmpvar_6.y) * 0.125)  * tmpvar_38)
			        ) + vec2(fragOffset_2 * tmpvar_39)
		        ) + vec2(0.5, 0.5)
	        ) * (1.0/(vec2(textureSize (a2ModesTex2,0))))
        );  
        fragColor = tmpvar_40;
        if (0 < monitorColorType)
        {
          float tmpvar_41;
          tmpvar_41 = sqrt(abs(dot (tmpvar_40.xyz, tmpvar_40.xyz)));
          if ((0.0 < tmpvar_41))
          {
            vec4 tmpvar_42;
            if ((monitorColorType < 2))
            {
              tmpvar_42 = vec4(0.0, 0.0, 0.0, 1.0);
              if (monitorColorType == 1) tmpvar_42 = vec4(1.0, 1.0, 1.0, 1.0);
            }
            else
            {
              tmpvar_42 = vec4(0.290196, 1.0, 0.0, 1.0);
              bvec2 tmpvar_43;
              tmpvar_43 = lessThanEqual (ivec2(monitorColorType), ivec2(3, 4));
              if (tmpvar_43.x) tmpvar_42 = vec4(1.0, 0.717647, 0.0, 1.0);
              if (tmpvar_43.y) tmpvar_42 = vec4(1.0, 0.0, 0.5, 1.0);
            }
fragColor = tmpvar_42;
          }
          else
          {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
          }
}
;
        tmpvar_1 = bool(1);
        tmpvar_9 = bool(1);
        tmpvar_10 = bool(0);
      }
      else
      {
        tmpvar_8 = (tmpvar_8 || (4u == tmpvar_7));
        if (tmpvar_8)
        {
          COMPAT_HIGHP uint byteValNext_44;
          COMPAT_HIGHP uint byteValPrev_45;
          if ((0 < monitorColorType))
          {
            vec4 tmpvar_46;
            if ((monitorColorType < 2))
            {
              tmpvar_46 = vec4(0.0, 0.0, 0.0, 1.0);
              if (monitorColorType == 1) tmpvar_46 = vec4(1.0, 1.0, 1.0, 1.0);
            }
            else
            {
              tmpvar_46 = vec4(0.290196, 1.0, 0.0, 1.0);
              bvec2 tmpvar_47;
              tmpvar_47 = lessThanEqual (ivec2(monitorColorType), ivec2(3, 4));
              if (tmpvar_47.x) tmpvar_46 = vec4(1.0, 0.717647, 0.0, 1.0);
              if (tmpvar_47.y) tmpvar_46 = vec4(1.0, 0.0, 0.5, 1.0);
            }
fragColor = (tmpvar_46 * float(min (max ((tmpvar_5.x & (1u << uint(
int((float((uint(mod ((tmpvar_3.x + -(uint((hborder * 14)))), 14u)))) * 0.5))
))), uint(0)), 1u)));
            tmpvar_1 = bool(1);
            tmpvar_9 = bool(1);
            tmpvar_10 = bool(0);
          }
          else
          {
            byteValPrev_45 = uint(0);
            byteValNext_44 = uint(0);
            int tmpvar_48;
            tmpvar_48 = int((float(int(tmpvar_3.x)) * 0.07142857));
            if ((-(tmpvar_48) < -(hborder)))
            {
              ivec2 tmpvar_49;
              tmpvar_49.x = (tmpvar_48 + -1);
              tmpvar_49.y = int(uint(int((float(tmpvar_3.y) * 0.5))));
              byteValPrev_45 = texelFetch (VRAMTEX, tmpvar_49, 0).x;
            };

            if (((tmpvar_48 + -(hborder)) < 39))
            {
              ivec2 tmpvar_50;
              tmpvar_50.x = (tmpvar_48 + 1);
              tmpvar_50.y = int(uint(int((float(tmpvar_3.y) * 0.5))));
              byteValNext_44 = texelFetch (VRAMTEX, tmpvar_50, 0).x;
            };
			
            int tmpvar_51;
            tmpvar_51 = ((int(((byteValPrev_45 & 224u) << 2)) | int(((byteValNext_44 & 3u) << 5))) + (((tmpvar_48 + -(hborder)) & 1) * 16));
            if ((0 < (specialModesMask & 6)))
            {
              uint tmpvar_52;
              tmpvar_52 = ((((((byteValNext_44 & 3u) << 9) | ((tmpvar_5.x & 127u) << 2)) | ((byteValPrev_45 & 127u) >> 5)) >> uint(int((float((fragOffset_2.x + 
                -((tmpvar_5.x >> 7))
              )) * 0.5)))) & 31u);
              if (((0 < (specialModesMask & 2)) && (tmpvar_52 == 27u)))
              {
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
                tmpvar_1 = bool(1);
                tmpvar_9 = bool(1);
                tmpvar_10 = bool(0);
              }
              else
              {
                bool tmpvar_53;
                tmpvar_53 = ((0 < (specialModesMask & 4)) && (tmpvar_52 == 4u));
                if (tmpvar_53) fragColor = vec4(1.0, 1.0, 1.0, 1.0);
                if (tmpvar_53) tmpvar_1 = bool(1);
                if (tmpvar_53) tmpvar_9 = bool(1);
                if (tmpvar_53) tmpvar_10 = bool(0);
              }
            };

            if (tmpvar_10)
            {
              vec2 tmpvar_54;
              tmpvar_54.x = float(tmpvar_51 + int(fragOffset_2.x));
              tmpvar_54.y = float(tmpvar_5.x);
              fragColor = texture (a2ModesTex3, (tmpvar_54 + vec2(0.5, 0.5)) * (1.0/vec2(textureSize (a2ModesTex3,0))));
              tmpvar_1 = bool(1);
              tmpvar_9 = bool(1);
              tmpvar_10 = bool(0);
            }
;
          }
}
;
        if (tmpvar_10)
        {
          tmpvar_8 = (tmpvar_8 || (5u == tmpvar_7));
          if (tmpvar_8)
          {
            COMPAT_HIGHP uint byteVal4_55;
            COMPAT_HIGHP uint byteVal1_56;
            if ((0 < monitorColorType))
            {
              vec4 tmpvar_57;
              if ((monitorColorType < 2))
              {
                tmpvar_57 = vec4(0.0, 0.0, 0.0, 1.0);
                if (monitorColorType == 1) tmpvar_57 = vec4(1.0, 1.0, 1.0, 1.0);
              }
              else
              {
                tmpvar_57 = vec4(0.290196, 1.0, 0.0, 1.0);
                bvec2 tmpvar_58;
                tmpvar_58 = lessThanEqual (ivec2(monitorColorType), ivec2(3, 4));
                if (tmpvar_58.x) tmpvar_57 = vec4(1.0, 0.717647, 0.0, 1.0);
                if (tmpvar_58.y) tmpvar_57 = vec4(1.0, 0.0, 0.5, 1.0);
              }
fragColor = (tmpvar_57 * float(min (max ((((tmpvar_5.x << 7) | (tmpvar_5.y & 127u)) & (1u << (uint(mod (
(tmpvar_3.x + -(uint((hborder * 14))))
, 14u))))), uint(0)), 1u)));
              tmpvar_1 = bool(1);
              tmpvar_9 = bool(1);
              tmpvar_10 = bool(0);
            }
            else
            {
              byteVal1_56 = uint(0);
              byteVal4_55 = uint(0);
              int tmpvar_59;
              tmpvar_59 = int((float(int(tmpvar_3.x)) * 0.07142857));
              if ((-(tmpvar_59) < -(hborder)))
              {
                ivec2 tmpvar_60;
                tmpvar_60.x = (tmpvar_59 + -1);
                tmpvar_60.y = int(uint(int((float(tmpvar_3.y) * 0.5))));
                byteVal1_56 = texelFetch (VRAMTEX, tmpvar_60, 0).x;
              }
;
              if (((tmpvar_59 + -(hborder)) < 39))
              {
                ivec2 tmpvar_61;
                tmpvar_61.x = (tmpvar_59 + 1);
                tmpvar_61.y = int(uint(int((float(tmpvar_3.y) * 0.5))));
                byteVal4_55 = texelFetch (VRAMTEX, tmpvar_61, 0).y;
              }
;
              if ((specialModesMask & 1) == 1)
              {
                COMPAT_HIGHP uint isColor_62;
                uint tmpvar_63;
                tmpvar_63 = tmpvar_3.x - uint(hborder * 14);
                int tmpvar_64;
                tmpvar_64 = int(
	                ( 
                      1 + mod(int(float(tmpvar_63) * 0.1428571), 2)
	                ) + 
	                min (
		              max (
			            int(mod(tmpvar_63, 7u) - mod(tmpvar_63, 4u))
		                  , -1
                        ), 0
	                )
                );				  
                isColor_62 = 1u;
                if (tmpvar_64 == 0)
                {
                  isColor_62 = (byteVal1_56 >> 7u);
                }
                else
                {
                  if (tmpvar_64 == 1)
                  {
                    isColor_62 = (tmpvar_5.y >> 7u);
                  }
                  else
                  {
                    isColor_62 = (tmpvar_5.x >> 7u);
                  }
}
                if (isColor_62 == uint(0))
                {
                  fragColor = vec4(float(min (max ((((tmpvar_5.x << 7) | (tmpvar_5.y & 127u)) & (1u << (uint(mod (tmpvar_63, 14u))))), uint(0)), 1u)));
                  tmpvar_1 = bool(1);
                  tmpvar_9 = bool(1);
                  tmpvar_10 = bool(0);
                };
              };
			   
 
              if (tmpvar_10)
              {
                int tmpvar_65;
                tmpvar_65 = ((((tmpvar_59 + -(hborder)) * 14) + int(fragOffset_2.x)) & 3);
                int tmpvar_66;
                tmpvar_66 = (((((int(byteVal1_56) & 112) | ((int(tmpvar_5.y) & 127) << 7)) | ((int(tmpvar_5.x) & 127) << 14)) | ((int(byteVal4_55) & 7) << 21)) >> ((4 + int(fragOffset_2.x)) + -(tmpvar_65)));
                vec2 tmpvar_67;
                tmpvar_67.x = float(((10 * ((tmpvar_66 >> 8) & 255)) + tmpvar_65));
                tmpvar_67.y = float((tmpvar_66 & 255));
                fragColor = texture (a2ModesTex4, ((tmpvar_67 + vec2(0.5, 0.5)) * (1.0/vec2(textureSize (a2ModesTex4,0)))));
                tmpvar_1 = bool(1);
                tmpvar_9 = bool(1);
                tmpvar_10 = bool(0);
              };
 
            }
          };
 
          if (tmpvar_10)
          {
            tmpvar_8 = (tmpvar_8 || (6u == tmpvar_7));
            if (tmpvar_8)
            {
              int tmpvar_68;
              tmpvar_68 = max (monitorColorType, 1);
              vec4 tmpvar_69;
              if ((tmpvar_68 < 2))
              {
                tmpvar_69 = vec4(0.0, 0.0, 0.0, 1.0);
                if (tmpvar_68 == 1) tmpvar_69 = vec4(1.0, 1.0, 1.0, 1.0);
              }
              else
              {
                tmpvar_69 = vec4(0.290196, 1.0, 0.0, 1.0);
                bvec2 tmpvar_70;
                tmpvar_70 = lessThanEqual (ivec2(tmpvar_68), ivec2(3, 4));
                if (tmpvar_70.x) tmpvar_69 = vec4(1.0, 0.717647, 0.0, 1.0);
                if (tmpvar_70.y) tmpvar_69 = vec4(1.0, 0.0, 0.5, 1.0);
              }
fragColor = (tmpvar_69 * float(min (max ((((tmpvar_5.x << 7) | (tmpvar_5.y & 127u)) & (1u << (uint(mod (
(tmpvar_3.x + -(uint((hborder * 14))))
, 14u))))), uint(0)), 1u)));
              tmpvar_1 = bool(1);
              tmpvar_9 = bool(1);
              tmpvar_10 = bool(0);
            }
            else
            {
              tmpvar_8 = (tmpvar_8 || (7u == tmpvar_7));
              if (tmpvar_8)
              {
                vec4 tmpvar_71;
                uint tmpvar_72;
                tmpvar_72 = ((tmpvar_5.z & 240u) >> 4);
                if ((tmpvar_72 < 8u))
                {
                  if ((tmpvar_72 < 4u))
                  {
                    tmpvar_71 = vec4(0.0, 0.0, 0.0, 1.0);
                    bvec3 tmpvar_73;
                    tmpvar_73 = lessThanEqual (uvec3(tmpvar_72), uvec3(1u, 2u, 3u));
                    if (tmpvar_73.x) tmpvar_71 = vec4(0.67451, 0.070588, 0.298039, 1.0);
                    if (tmpvar_73.y) tmpvar_71 = vec4(0.0, 0.027451, 0.513725, 1.0);
                    if (tmpvar_73.z) tmpvar_71 = vec4(0.666667, 0.101961, 0.819608, 1.0);
                  }
                  else
                  {
                    tmpvar_71 = vec4(0.0, 0.513725, 0.184314, 1.0);
                    bvec3 tmpvar_74;
                    tmpvar_74 = lessThanEqual (uvec3(tmpvar_72), uvec3(5u, 6u, 7u));
                    if (tmpvar_74.x) tmpvar_71 = vec4(0.623529, 0.592157, 0.494118, 1.0);
                    if (tmpvar_74.y) tmpvar_71 = vec4(0.0, 0.541176, 0.709804, 1.0);
                    if (tmpvar_74.z) tmpvar_71 = vec4(0.623529, 0.619608, 1.0, 1.0);
                  }
}
else
{
                  if ((tmpvar_72 < 12u))
                  {
                    tmpvar_71 = vec4(0.478431, 0.372549, 0.0, 1.0);
                    bvec3 tmpvar_75;
                    tmpvar_75 = lessThanEqual (uvec3(tmpvar_72), uvec3(9u, 10u, 11u));
                    if (tmpvar_75.x) tmpvar_71 = vec4(1.0, 0.447059, 0.278431, 1.0);
                    if (tmpvar_75.y) tmpvar_71 = vec4(0.470588, 0.407843, 0.498039, 1.0);
                    if (tmpvar_75.z) tmpvar_71 = vec4(1.0, 0.478431, 0.811765, 1.0);
                  }
                  else
                  {
                    tmpvar_71 = vec4(0.435294, 0.901961, 0.172549, 1.0);
                    bvec3 tmpvar_76;
                    tmpvar_76 = lessThanEqual (uvec3(tmpvar_72), uvec3(13u, 14u, 15u));
                    if (tmpvar_76.x) tmpvar_71 = vec4(1.0, 0.964706, 0.482353, 1.0);
                    if (tmpvar_76.y) tmpvar_71 = vec4(0.423529, 0.933333, 0.698039, 1.0);
                    if (tmpvar_76.z) tmpvar_71 = vec4(1.0, 1.0, 1.0, 1.0);
                  }
}
fragColor = tmpvar_71;
                if ((0 < monitorColorType))
                {
                  if ((uint(0) < ((tmpvar_5.z & 240u) >> 4)))
                  {
                    vec4 tmpvar_77;
                    if (monitorColorType < 2)
                    {
                      tmpvar_77 = vec4(0.0, 0.0, 0.0, 1.0);
                      if (monitorColorType == 1) tmpvar_77 = vec4(1.0, 1.0, 1.0, 1.0);
                    }
                    else
                    {
                      tmpvar_77 = vec4(0.290196, 1.0, 0.0, 1.0);
                      bvec2 tmpvar_78;
                      tmpvar_78 = lessThanEqual (ivec2(monitorColorType), ivec2(3, 4));
                      if (tmpvar_78.x) tmpvar_77 = vec4(1.0, 0.717647, 0.0, 1.0);
                      if (tmpvar_78.y) tmpvar_77 = vec4(1.0, 0.0, 0.5, 1.0);
                    }
                    fragColor = tmpvar_77;
                  }
                  else
                  {
                    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
                  }
}
;
                tmpvar_1 = bool(1);
                tmpvar_9 = bool(1);
                tmpvar_10 = bool(0);
              }
              else
              {
                tmpvar_8 = bool(1);
                if (tmpvar_8) fragColor = vec4(1.0, 0.0, 0.5, 1.0);
                if (tmpvar_8) tmpvar_1 = bool(1);
                if (tmpvar_8) tmpvar_9 = bool(1);
                if (tmpvar_8) tmpvar_10 = bool(0);
                bool tmpvar_79 = bool(0);
                tmpvar_79 = bool(0);
                if (tmpvar_79) tmpvar_9 = bool(1);
                if (tmpvar_79) tmpvar_10 = bool(0);
              }
}
}
;
        }
;
      }
}
if (tmpvar_9)
{
      break;
    }
;
  }
;
  bool tmpvar_80;
  tmpvar_80 = tmpvar_1;
  if (tmpvar_1) tmpvar_1 = bool(1);
  if (!(tmpvar_80)) fragColor = vec4(0.0, 1.0, 0.5, 1.0);
}

