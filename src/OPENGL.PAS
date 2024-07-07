unit Opengl;

interface

uses Windows;

type
  HGLRC = THandle;

{
/*
** Copyright 1991-1993, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
*/
}

type
  GLenum = Cardinal;
  GLboolean = BYTEBOOL;
  GLbitfield = Cardinal;
  GLbyte = Shortint;   { signed char }
  GLshort = SmallInt;
  GLint = Integer;
  GLsizei = Integer;
  GLubyte = Byte;
  GLushort = Word;
  GLuint = Cardinal;
  GLfloat = Single;
  GLclampf = Single;
  GLdouble = Double;
  GLclampd = Double;

  PGLBoolean = ^GLBoolean;
  PGLByte = ^GLByte;
  PGLShort = ^GLShort;
  PGLInt = ^GLInt;
  PGLSizei = ^GLSizei;
  PGLubyte = ^GLubyte;
  PGLushort = ^GLushort;
  PGLuint = ^GLuint;
  PGLclampf = ^GLclampf;
  PGLfloat =  ^GLFloat;
  PGLdouble = ^GLDouble;
  PGLclampd = ^GLclampd;

  TGLArrayf4 = array [0..3] of GLFloat;
  TGLArrayf3 = array [0..2] of GLFloat;
  TGLArrayi4 = array [0..3] of GLint;
  {...}

{*************************************************************}

const
{ AttribMask }
  GL_CURRENT_BIT                      = $00000001;
  GL_POINT_BIT                        = $00000002;
  GL_LINE_BIT                         = $00000004;
  GL_POLYGON_BIT                      = $00000008;
  GL_POLYGON_STIPPLE_BIT              = $00000010;
  GL_PIXEL_MODE_BIT                   = $00000020;
  GL_LIGHTING_BIT                     = $00000040;
  GL_FOG_BIT                          = $00000080;
  GL_DEPTH_BUFFER_BIT                 = $00000100;
  GL_ACCUM_BUFFER_BIT                 = $00000200;
  GL_STENCIL_BUFFER_BIT               = $00000400;
  GL_VIEWPORT_BIT                     = $00000800;
  GL_TRANSFORM_BIT                    = $00001000;
  GL_ENABLE_BIT                       = $00002000;
  GL_COLOR_BUFFER_BIT                 = $00004000;
  GL_HINT_BIT                         = $00008000;
  GL_EVAL_BIT                         = $00010000;
  GL_LIST_BIT                         = $00020000;
  GL_TEXTURE_BIT                      = $00040000;
  GL_SCISSOR_BIT                      = $00080000;
  GL_ALL_ATTRIB_BITS                  = $000fffff;

{ ClearBufferMask }
{      GL_COLOR_BUFFER_BIT }
{      GL_ACCUM_BUFFER_BIT }
{      GL_STENCIL_BUFFER_BIT }
{      GL_DEPTH_BUFFER_BIT }

{ Boolean }
  GL_FALSE                            = 0;
  GL_TRUE                             = 1;

{ BeginMode }
  GL_POINTS                           = $0000    ;
  GL_LINES                            = $0001    ;
  GL_LINE_LOOP                        = $0002    ;
  GL_LINE_STRIP                       = $0003    ;
  GL_TRIANGLES                        = $0004    ;
  GL_TRIANGLE_STRIP                   = $0005    ;
  GL_TRIANGLE_FAN                     = $0006    ;
  GL_QUADS                            = $0007    ;
  GL_QUAD_STRIP                       = $0008    ;
  GL_POLYGON                          = $0009    ;

{ AccumOp }
  GL_ACCUM                            = $0100;
  GL_LOAD                             = $0101;
  GL_RETURN                           = $0102;
  GL_MULT                             = $0103;
  GL_ADD                              = $0104;

{ AlphaFunction }
  GL_NEVER                            = $0200;
  GL_LESS                             = $0201;
  GL_EQUAL                            = $0202;
  GL_LEQUAL                           = $0203;
  GL_GREATER                          = $0204;
  GL_NOTEQUAL                         = $0205;
  GL_GEQUAL                           = $0206;
  GL_ALWAYS                           = $0207;

{ BlendingFactorDest }
  GL_ZERO                             = 0;
  GL_ONE                              = 1;
  GL_SRC_COLOR                        = $0300;
  GL_ONE_MINUS_SRC_COLOR              = $0301;
  GL_SRC_ALPHA                        = $0302;
  GL_ONE_MINUS_SRC_ALPHA              = $0303;
  GL_DST_ALPHA                        = $0304;
  GL_ONE_MINUS_DST_ALPHA              = $0305;

{ BlendingFactorSrc }
{      GL_ZERO }
{      GL_ONE }
  GL_DST_COLOR                        = $0306;
  GL_ONE_MINUS_DST_COLOR              = $0307;
  GL_SRC_ALPHA_SATURATE               = $0308;
{      GL_SRC_ALPHA }
{      GL_ONE_MINUS_SRC_ALPHA }
{      GL_DST_ALPHA }
{      GL_ONE_MINUS_DST_ALPHA }

{ BlendingMode }
{      GL_LOGIC_OP }

{ ColorMaterialFace }
{      GL_FRONT }
{      GL_BACK }
{      GL_FRONT_AND_BACK }

{ ColorMaterialParameter }
{      GL_AMBIENT }
{      GL_DIFFUSE }
{      GL_SPECULAR }
{      GL_EMISSION }
{      GL_AMBIENT_AND_DIFFUSE }

{ CullFaceMode }
{      GL_FRONT }
{      GL_BACK }
{      GL_FRONT_AND_BACK }

{ DepthFunction }
{      GL_NEVER }
{      GL_LESS }
{      GL_EQUAL }
{      GL_LEQUAL }
{      GL_GREATER }
{      GL_NOTEQUAL }
{      GL_GEQUAL }
{      GL_ALWAYS }

{ DrawBufferMode }
  GL_NONE                             = 0;
  GL_FRONT_LEFT                       = $0400;
  GL_FRONT_RIGHT                      = $0401;
  GL_BACK_LEFT                        = $0402;
  GL_BACK_RIGHT                       = $0403;
  GL_FRONT                            = $0404;
  GL_BACK                             = $0405;
  GL_LEFT                             = $0406;
  GL_RIGHT                            = $0407;
  GL_FRONT_AND_BACK                   = $0408;
  GL_AUX0                             = $0409;
  GL_AUX1                             = $040A;
  GL_AUX2                             = $040B;
  GL_AUX3                             = $040C;

{ ErrorCode }
  GL_NO_ERROR                         = 0;
  GL_INVALID_ENUM                     = $0500;
  GL_INVALID_VALUE                    = $0501;
  GL_INVALID_OPERATION                = $0502;
  GL_STACK_OVERFLOW                   = $0503;
  GL_STACK_UNDERFLOW                  = $0504;
  GL_OUT_OF_MEMORY                    = $0505;

{ FeedBackMode }
  GL_2D                               = $0600;
  GL_3D                               = $0601;
  GL_3D_COLOR                         = $0602;
  GL_3D_COLOR_TEXTURE                 = $0603;
  GL_4D_COLOR_TEXTURE                 = $0604;

{ FeedBackToken }
  GL_PASS_THROUGH_TOKEN               = $0700;
  GL_POINT_TOKEN                      = $0701;
  GL_LINE_TOKEN                       = $0702;
  GL_POLYGON_TOKEN                    = $0703;
  GL_BITMAP_TOKEN                     = $0704;
  GL_DRAW_PIXEL_TOKEN                 = $0705;
  GL_COPY_PIXEL_TOKEN                 = $0706;
  GL_LINE_RESET_TOKEN                 = $0707;

{ FogMode }
{      GL_LINEAR }
  GL_EXP                              = $0800;
  GL_EXP2                             = $0801;

{ FogParameter }
{      GL_FOG_COLOR }
{      GL_FOG_DENSITY }
{      GL_FOG_END }
{      GL_FOG_INDEX }
{      GL_FOG_MODE }
{      GL_FOG_START }

{ FrontFaceDirection }
  GL_CW                               = $0900;
  GL_CCW                              = $0901;

{ GetMapTarget }
  GL_COEFF                            = $0A00;
  GL_ORDER                            = $0A01;
  GL_DOMAIN                           = $0A02;

{ GetPixelMap }
  GL_PIXEL_MAP_I_TO_I                 = $0C70;
  GL_PIXEL_MAP_S_TO_S                 = $0C71;
  GL_PIXEL_MAP_I_TO_R                 = $0C72;
  GL_PIXEL_MAP_I_TO_G                 = $0C73;
  GL_PIXEL_MAP_I_TO_B                 = $0C74;
  GL_PIXEL_MAP_I_TO_A                 = $0C75;
  GL_PIXEL_MAP_R_TO_R                 = $0C76;
  GL_PIXEL_MAP_G_TO_G                 = $0C77;
  GL_PIXEL_MAP_B_TO_B                 = $0C78;
  GL_PIXEL_MAP_A_TO_A                 = $0C79;

{ GetTarget }
  GL_CURRENT_COLOR                    = $0B00;
  GL_CURRENT_INDEX                    = $0B01;
  GL_CURRENT_NORMAL                   = $0B02;
  GL_CURRENT_TEXTURE_COORDS           = $0B03;
  GL_CURRENT_RASTER_COLOR             = $0B04;
  GL_CURRENT_RASTER_INDEX             = $0B05;
  GL_CURRENT_RASTER_TEXTURE_COORDS    = $0B06;
  GL_CURRENT_RASTER_POSITION          = $0B07;
  GL_CURRENT_RASTER_POSITION_VALID    = $0B08;
  GL_CURRENT_RASTER_DISTANCE          = $0B09;
  GL_POINT_SMOOTH                     = $0B10;
  GL_POINT_SIZE                       = $0B11;
  GL_POINT_SIZE_RANGE                 = $0B12;
  GL_POINT_SIZE_GRANULARITY           = $0B13;
  GL_LINE_SMOOTH                      = $0B20;
  GL_LINE_WIDTH                       = $0B21;
  GL_LINE_WIDTH_RANGE                 = $0B22;
  GL_LINE_WIDTH_GRANULARITY           = $0B23;
  GL_LINE_STIPPLE                     = $0B24;
  GL_LINE_STIPPLE_PATTERN             = $0B25;
  GL_LINE_STIPPLE_REPEAT              = $0B26;
  GL_LIST_MODE                        = $0B30;
  GL_MAX_LIST_NESTING                 = $0B31;
  GL_LIST_BASE                        = $0B32;
  GL_LIST_INDEX                       = $0B33;
  GL_POLYGON_MODE                     = $0B40;
  GL_POLYGON_SMOOTH                   = $0B41;
  GL_POLYGON_STIPPLE                  = $0B42;
  GL_EDGE_FLAG                        = $0B43;
  GL_CULL_FACE                        = $0B44;
  GL_CULL_FACE_MODE                   = $0B45;
  GL_FRONT_FACE                       = $0B46;
  GL_LIGHTING                         = $0B50;
  GL_LIGHT_MODEL_LOCAL_VIEWER         = $0B51;
  GL_LIGHT_MODEL_TWO_SIDE             = $0B52;
  GL_LIGHT_MODEL_AMBIENT              = $0B53;
  GL_SHADE_MODEL                      = $0B54;
  GL_COLOR_MATERIAL_FACE              = $0B55;
  GL_COLOR_MATERIAL_PARAMETER         = $0B56;
  GL_COLOR_MATERIAL                   = $0B57;
  GL_FOG                              = $0B60;
  GL_FOG_INDEX                        = $0B61;
  GL_FOG_DENSITY                      = $0B62;
  GL_FOG_START                        = $0B63;
  GL_FOG_END                          = $0B64;
  GL_FOG_MODE                         = $0B65;
  GL_FOG_COLOR                        = $0B66;
  GL_DEPTH_RANGE                      = $0B70;
  GL_DEPTH_TEST                       = $0B71;
  GL_DEPTH_WRITEMASK                  = $0B72;
  GL_DEPTH_CLEAR_VALUE                = $0B73;
  GL_DEPTH_FUNC                       = $0B74;
  GL_ACCUM_CLEAR_VALUE                = $0B80;
  GL_STENCIL_TEST                     = $0B90;
  GL_STENCIL_CLEAR_VALUE              = $0B91;
  GL_STENCIL_FUNC                     = $0B92;
  GL_STENCIL_VALUE_MASK               = $0B93;
  GL_STENCIL_FAIL                     = $0B94;
  GL_STENCIL_PASS_DEPTH_FAIL          = $0B95;
  GL_STENCIL_PASS_DEPTH_PASS          = $0B96;
  GL_STENCIL_REF                      = $0B97;
  GL_STENCIL_WRITEMASK                = $0B98;
  GL_MATRIX_MODE                      = $0BA0;
  GL_NORMALIZE                        = $0BA1;
  GL_VIEWPORT                         = $0BA2;
  GL_MODELVIEW_STACK_DEPTH            = $0BA3;
  GL_PROJECTION_STACK_DEPTH           = $0BA4;
  GL_TEXTURE_STACK_DEPTH              = $0BA5;
  GL_MODELVIEW_MATRIX                 = $0BA6;
  GL_PROJECTION_MATRIX                = $0BA7;
  GL_TEXTURE_MATRIX                   = $0BA8;
  GL_ATTRIB_STACK_DEPTH               = $0BB0;
  GL_ALPHA_TEST                       = $0BC0;
  GL_ALPHA_TEST_FUNC                  = $0BC1;
  GL_ALPHA_TEST_REF                   = $0BC2;
  GL_DITHER                           = $0BD0;
  GL_BLEND_DST                        = $0BE0;
  GL_BLEND_SRC                        = $0BE1;
  GL_BLEND                            = $0BE2;
  GL_LOGIC_OP_MODE                    = $0BF0;
  GL_LOGIC_OP                         = $0BF1;
  GL_AUX_BUFFERS                      = $0C00;
  GL_DRAW_BUFFER                      = $0C01;
  GL_READ_BUFFER                      = $0C02;
  GL_SCISSOR_BOX                      = $0C10;
  GL_SCISSOR_TEST                     = $0C11;
  GL_INDEX_CLEAR_VALUE                = $0C20;
  GL_INDEX_WRITEMASK                  = $0C21;
  GL_COLOR_CLEAR_VALUE                = $0C22;
  GL_COLOR_WRITEMASK                  = $0C23;
  GL_INDEX_MODE                       = $0C30;
  GL_RGBA_MODE                        = $0C31;
  GL_DOUBLEBUFFER                     = $0C32;
  GL_STEREO                           = $0C33;
  GL_RENDER_MODE                      = $0C40;
  GL_PERSPECTIVE_CORRECTION_HINT      = $0C50;
  GL_POINT_SMOOTH_HINT                = $0C51;
  GL_LINE_SMOOTH_HINT                 = $0C52;
  GL_POLYGON_SMOOTH_HINT              = $0C53;
  GL_FOG_HINT                         = $0C54;
  GL_TEXTURE_GEN_S                    = $0C60;
  GL_TEXTURE_GEN_T                    = $0C61;
  GL_TEXTURE_GEN_R                    = $0C62;
  GL_TEXTURE_GEN_Q                    = $0C63;
  GL_PIXEL_MAP_I_TO_I_SIZE            = $0CB0;
  GL_PIXEL_MAP_S_TO_S_SIZE            = $0CB1;
  GL_PIXEL_MAP_I_TO_R_SIZE            = $0CB2;
  GL_PIXEL_MAP_I_TO_G_SIZE            = $0CB3;
  GL_PIXEL_MAP_I_TO_B_SIZE            = $0CB4;
  GL_PIXEL_MAP_I_TO_A_SIZE            = $0CB5;
  GL_PIXEL_MAP_R_TO_R_SIZE            = $0CB6;
  GL_PIXEL_MAP_G_TO_G_SIZE            = $0CB7;
  GL_PIXEL_MAP_B_TO_B_SIZE            = $0CB8;
  GL_PIXEL_MAP_A_TO_A_SIZE            = $0CB9;
  GL_UNPACK_SWAP_BYTES                = $0CF0;
  GL_UNPACK_LSB_FIRST                 = $0CF1;
  GL_UNPACK_ROW_LENGTH                = $0CF2;
  GL_UNPACK_SKIP_ROWS                 = $0CF3;
  GL_UNPACK_SKIP_PIXELS               = $0CF4;
  GL_UNPACK_ALIGNMENT                 = $0CF5;
  GL_PACK_SWAP_BYTES                  = $0D00;
  GL_PACK_LSB_FIRST                   = $0D01;
  GL_PACK_ROW_LENGTH                  = $0D02;
  GL_PACK_SKIP_ROWS                   = $0D03;
  GL_PACK_SKIP_PIXELS                 = $0D04;
  GL_PACK_ALIGNMENT                   = $0D05;
  GL_MAP_COLOR                        = $0D10;
  GL_MAP_STENCIL                      = $0D11;
  GL_INDEX_SHIFT                      = $0D12;
  GL_INDEX_OFFSET                     = $0D13;
  GL_RED_SCALE                        = $0D14;
  GL_RED_BIAS                         = $0D15;
  GL_ZOOM_X                           = $0D16;
  GL_ZOOM_Y                           = $0D17;
  GL_GREEN_SCALE                      = $0D18;
  GL_GREEN_BIAS                       = $0D19;
  GL_BLUE_SCALE                       = $0D1A;
  GL_BLUE_BIAS                        = $0D1B;
  GL_ALPHA_SCALE                      = $0D1C;
  GL_ALPHA_BIAS                       = $0D1D;
  GL_DEPTH_SCALE                      = $0D1E;
  GL_DEPTH_BIAS                       = $0D1F;
  GL_MAX_EVAL_ORDER                   = $0D30;
  GL_MAX_LIGHTS                       = $0D31;
  GL_MAX_CLIP_PLANES                  = $0D32;
  GL_MAX_TEXTURE_SIZE                 = $0D33;
  GL_MAX_PIXEL_MAP_TABLE              = $0D34;
  GL_MAX_ATTRIB_STACK_DEPTH           = $0D35;
  GL_MAX_MODELVIEW_STACK_DEPTH        = $0D36;
  GL_MAX_NAME_STACK_DEPTH             = $0D37;
  GL_MAX_PROJECTION_STACK_DEPTH       = $0D38;
  GL_MAX_TEXTURE_STACK_DEPTH          = $0D39;
  GL_MAX_VIEWPORT_DIMS                = $0D3A;
  GL_SUBPIXEL_BITS                    = $0D50;
  GL_INDEX_BITS                       = $0D51;
  GL_RED_BITS                         = $0D52;
  GL_GREEN_BITS                       = $0D53;
  GL_BLUE_BITS                        = $0D54;
  GL_ALPHA_BITS                       = $0D55;
  GL_DEPTH_BITS                       = $0D56;
  GL_STENCIL_BITS                     = $0D57;
  GL_ACCUM_RED_BITS                   = $0D58;
  GL_ACCUM_GREEN_BITS                 = $0D59;
  GL_ACCUM_BLUE_BITS                  = $0D5A;
  GL_ACCUM_ALPHA_BITS                 = $0D5B;
  GL_NAME_STACK_DEPTH                 = $0D70;
  GL_AUTO_NORMAL                      = $0D80;
  GL_MAP1_COLOR_4                     = $0D90;
  GL_MAP1_INDEX                       = $0D91;
  GL_MAP1_NORMAL                      = $0D92;
  GL_MAP1_TEXTURE_COORD_1             = $0D93;
  GL_MAP1_TEXTURE_COORD_2             = $0D94;
  GL_MAP1_TEXTURE_COORD_3             = $0D95;
  GL_MAP1_TEXTURE_COORD_4             = $0D96;
  GL_MAP1_VERTEX_3                    = $0D97;
  GL_MAP1_VERTEX_4                    = $0D98;
  GL_MAP2_COLOR_4                     = $0DB0;
  GL_MAP2_INDEX                       = $0DB1;
  GL_MAP2_NORMAL                      = $0DB2;
  GL_MAP2_TEXTURE_COORD_1             = $0DB3;
  GL_MAP2_TEXTURE_COORD_2             = $0DB4;
  GL_MAP2_TEXTURE_COORD_3             = $0DB5;
  GL_MAP2_TEXTURE_COORD_4             = $0DB6;
  GL_MAP2_VERTEX_3                    = $0DB7;
  GL_MAP2_VERTEX_4                    = $0DB8;
  GL_MAP1_GRID_DOMAIN                 = $0DD0;
  GL_MAP1_GRID_SEGMENTS               = $0DD1;
  GL_MAP2_GRID_DOMAIN                 = $0DD2;
  GL_MAP2_GRID_SEGMENTS               = $0DD3;
  GL_TEXTURE_1D                       = $0DE0;
  GL_TEXTURE_2D                       = $0DE1;

{ GetTextureParameter }
{      GL_TEXTURE_MAG_FILTER }
{      GL_TEXTURE_MIN_FILTER }
{      GL_TEXTURE_WRAP_S }
{      GL_TEXTURE_WRAP_T }
  GL_TEXTURE_WIDTH                    = $1000;
  GL_TEXTURE_HEIGHT                   = $1001;
  GL_TEXTURE_COMPONENTS               = $1003;
  GL_TEXTURE_BORDER_COLOR             = $1004;
  GL_TEXTURE_BORDER                   = $1005;

{ HintMode }
  GL_DONT_CARE                        = $1100;
  GL_FASTEST                          = $1101;
  GL_NICEST                           = $1102;

{ HintTarget }
{      GL_PERSPECTIVE_CORRECTION_HINT }
{      GL_POINT_SMOOTH_HINT }
{      GL_LINE_SMOOTH_HINT }
{      GL_POLYGON_SMOOTH_HINT }
{      GL_FOG_HINT }

{ LightModelParameter }
{      GL_LIGHT_MODEL_AMBIENT }
{      GL_LIGHT_MODEL_LOCAL_VIEWER }
{      GL_LIGHT_MODEL_TWO_SIDE }

{ LightParameter }
  GL_AMBIENT                          = $1200;
  GL_DIFFUSE                          = $1201;
  GL_SPECULAR                         = $1202;
  GL_POSITION                         = $1203;
  GL_SPOT_DIRECTION                   = $1204;
  GL_SPOT_EXPONENT                    = $1205;
  GL_SPOT_CUTOFF                      = $1206;
  GL_CONSTANT_ATTENUATION             = $1207;
  GL_LINEAR_ATTENUATION               = $1208;
  GL_QUADRATIC_ATTENUATION            = $1209;

{ ListMode }
  GL_COMPILE                          = $1300;
  GL_COMPILE_AND_EXECUTE              = $1301;

{ ListNameType }
  GL_BYTE                             = $1400;
  GL_UNSIGNED_BYTE                    = $1401;
  GL_SHORT                            = $1402;
  GL_UNSIGNED_SHORT                   = $1403;
  GL_INT                              = $1404;
  GL_UNSIGNED_INT                     = $1405;
  GL_FLOAT                            = $1406;
  GL_2_BYTES                          = $1407;
  GL_3_BYTES                          = $1408;
  GL_4_BYTES                          = $1409;

{ LogicOp }
  GL_CLEAR                            = $1500;
  GL_AND                              = $1501;
  GL_AND_REVERSE                      = $1502;
  GL_COPY                             = $1503;
  GL_AND_INVERTED                     = $1504;
  GL_NOOP                             = $1505;
  GL_XOR                              = $1506;
  GL_OR                               = $1507;
  GL_NOR                              = $1508;
  GL_EQUIV                            = $1509;
  GL_INVERT                           = $150A;
  GL_OR_REVERSE                       = $150B;
  GL_COPY_INVERTED                    = $150C;
  GL_OR_INVERTED                      = $150D;
  GL_NAND                             = $150E;
  GL_SET                              = $150F;

{ MapTarget }
{      GL_MAP1_COLOR_4 }
{      GL_MAP1_INDEX }
{      GL_MAP1_NORMAL }
{      GL_MAP1_TEXTURE_COORD_1 }
{      GL_MAP1_TEXTURE_COORD_2 }
{      GL_MAP1_TEXTURE_COORD_3 }
{      GL_MAP1_TEXTURE_COORD_4 }
{      GL_MAP1_VERTEX_3 }
{      GL_MAP1_VERTEX_4 }
{      GL_MAP2_COLOR_4 }
{      GL_MAP2_INDEX }
{      GL_MAP2_NORMAL }
{      GL_MAP2_TEXTURE_COORD_1 }
{      GL_MAP2_TEXTURE_COORD_2 }
{      GL_MAP2_TEXTURE_COORD_3 }
{      GL_MAP2_TEXTURE_COORD_4 }
{      GL_MAP2_VERTEX_3 }
{      GL_MAP2_VERTEX_4 }

{ MaterialFace }
{      GL_FRONT }
{      GL_BACK }
{      GL_FRONT_AND_BACK }

{ MaterialParameter }
  GL_EMISSION                         = $1600;
  GL_SHININESS                        = $1601;
  GL_AMBIENT_AND_DIFFUSE              = $1602;
  GL_COLOR_INDEXES                    = $1603;
{      GL_AMBIENT }
{      GL_DIFFUSE }
{      GL_SPECULAR }

{ MatrixMode }
  GL_MODELVIEW                        = $1700;
  GL_PROJECTION                       = $1701;
  GL_TEXTURE                          = $1702;

{ MeshMode1 }
{      GL_POINT }
{      GL_LINE }

{ MeshMode2 }
{      GL_POINT }
{      GL_LINE }
{      GL_FILL }

{ PixelCopyType }
  GL_COLOR                            = $1800;
  GL_DEPTH                            = $1801;
  GL_STENCIL                          = $1802;

{ PixelFormat }
  GL_COLOR_INDEX                      = $1900;
  GL_STENCIL_INDEX                    = $1901;
  GL_DEPTH_COMPONENT                  = $1902;
  GL_RED                              = $1903;
  GL_GREEN                            = $1904;
  GL_BLUE                             = $1905;
  GL_ALPHA                            = $1906;
  GL_RGB                              = $1907;
  GL_RGBA                             = $1908;
  GL_LUMINANCE                        = $1909;
  GL_LUMINANCE_ALPHA                  = $190A;

{ PixelMap }
{      GL_PIXEL_MAP_I_TO_I }
{      GL_PIXEL_MAP_S_TO_S }
{      GL_PIXEL_MAP_I_TO_R }
{      GL_PIXEL_MAP_I_TO_G }
{      GL_PIXEL_MAP_I_TO_B }
{      GL_PIXEL_MAP_I_TO_A }
{      GL_PIXEL_MAP_R_TO_R }
{      GL_PIXEL_MAP_G_TO_G }
{      GL_PIXEL_MAP_B_TO_B }
{      GL_PIXEL_MAP_A_TO_A }

{ PixelStore }
{      GL_UNPACK_SWAP_BYTES }
{      GL_UNPACK_LSB_FIRST }
{      GL_UNPACK_ROW_LENGTH }
{      GL_UNPACK_SKIP_ROWS }
{      GL_UNPACK_SKIP_PIXELS }
{      GL_UNPACK_ALIGNMENT }
{      GL_PACK_SWAP_BYTES }
{      GL_PACK_LSB_FIRST }
{      GL_PACK_ROW_LENGTH }
{      GL_PACK_SKIP_ROWS }
{      GL_PACK_SKIP_PIXELS }
{      GL_PACK_ALIGNMENT }

{ PixelTransfer }
{      GL_MAP_COLOR }
{      GL_MAP_STENCIL }
{      GL_INDEX_SHIFT }
{      GL_INDEX_OFFSET }
{      GL_RED_SCALE }
{      GL_RED_BIAS }
{      GL_GREEN_SCALE }
{      GL_GREEN_BIAS }
{      GL_BLUE_SCALE }
{      GL_BLUE_BIAS }
{      GL_ALPHA_SCALE }
{      GL_ALPHA_BIAS }
{      GL_DEPTH_SCALE }
{      GL_DEPTH_BIAS }

{ PixelType }
  GL_BITMAP                           = $1A00;
{      GL_BYTE }
{      GL_UNSIGNED_BYTE }
{      GL_SHORT }
{      GL_UNSIGNED_SHORT }
{      GL_INT }
{      GL_UNSIGNED_INT }
{      GL_FLOAT }

{ PolygonMode }
  GL_POINT                            = $1B00;
  GL_LINE                             = $1B01;
  GL_FILL                             = $1B02;

{ ReadBufferMode }
{      GL_FRONT_LEFT }
{      GL_FRONT_RIGHT }
{      GL_BACK_LEFT }
{      GL_BACK_RIGHT }
{      GL_FRONT }
{      GL_BACK }
{      GL_LEFT }
{      GL_RIGHT }
{      GL_AUX0 }
{      GL_AUX1 }
{      GL_AUX2 }
{      GL_AUX3 }

{ RenderingMode }
  GL_RENDER                           = $1C00;
  GL_FEEDBACK                         = $1C01;
  GL_SELECT                           = $1C02;

{ ShadingModel }
  GL_FLAT                             = $1D00;
  GL_SMOOTH                           = $1D01;

{ StencilFunction }
{      GL_NEVER }
{      GL_LESS }
{      GL_EQUAL }
{      GL_LEQUAL }
{      GL_GREATER }
{      GL_NOTEQUAL }
{      GL_GEQUAL }
{      GL_ALWAYS }

{ StencilOp }
{      GL_ZERO }
  GL_KEEP                             = $1E00;
  GL_REPLACE                          = $1E01;
  GL_INCR                             = $1E02;
  GL_DECR                             = $1E03;
{      GL_INVERT }

{ StringName }
  GL_VENDOR                           = $1F00;
  GL_RENDERER                         = $1F01;
  GL_VERSION                          = $1F02;
  GL_EXTENSIONS                       = $1F03;

{ TextureCoordName }
  GL_S                                = $2000;
  GL_T                                = $2001;
  GL_R                                = $2002;
  GL_Q                                = $2003;

{ TextureEnvMode }
  GL_MODULATE                         = $2100;
  GL_DECAL                            = $2101;
{      GL_BLEND }

{ TextureEnvParameter }
  GL_TEXTURE_ENV_MODE                 = $2200;
  GL_TEXTURE_ENV_COLOR                = $2201;

{ TextureEnvTarget }
  GL_TEXTURE_ENV                      = $2300;

{ TextureGenMode }
  GL_EYE_LINEAR                       = $2400;
  GL_OBJECT_LINEAR                    = $2401;
  GL_SPHERE_MAP                       = $2402;

{ TextureGenParameter }
  GL_TEXTURE_GEN_MODE                 = $2500;
  GL_OBJECT_PLANE                     = $2501;
  GL_EYE_PLANE                        = $2502;

{ TextureMagFilter }
  GL_NEAREST                          = $2600;
  GL_LINEAR                           = $2601;

{ TextureMinFilter }
{      GL_NEAREST }
{      GL_LINEAR }
  GL_NEAREST_MIPMAP_NEAREST           = $2700;
  GL_LINEAR_MIPMAP_NEAREST            = $2701;
  GL_NEAREST_MIPMAP_LINEAR            = $2702;
  GL_LINEAR_MIPMAP_LINEAR             = $2703;

{ TextureParameterName }
  GL_TEXTURE_MAG_FILTER               = $2800;
  GL_TEXTURE_MIN_FILTER               = $2801;
  GL_TEXTURE_WRAP_S                   = $2802;
  GL_TEXTURE_WRAP_T                   = $2803;
{      GL_TEXTURE_BORDER_COLOR }

{ TextureTarget }
{      GL_TEXTURE_1D }
{      GL_TEXTURE_2D }

{ TextureWrapMode }
  GL_CLAMP                            = $2900;
  GL_REPEAT                           = $2901;

{ ClipPlaneName }
  GL_CLIP_PLANE0                      = $3000;
  GL_CLIP_PLANE1                      = $3001;
  GL_CLIP_PLANE2                      = $3002;
  GL_CLIP_PLANE3                      = $3003;
  GL_CLIP_PLANE4                      = $3004;
  GL_CLIP_PLANE5                      = $3005;

{ LightName }
  GL_LIGHT0                           = $4000;
  GL_LIGHT1                           = $4001;
  GL_LIGHT2                           = $4002;
  GL_LIGHT3                           = $4003;
  GL_LIGHT4                           = $4004;
  GL_LIGHT5                           = $4005;
  GL_LIGHT6                           = $4006;
  GL_LIGHT7                           = $4007;

// Extensions
  GL_EXT_vertex_array                 = 1;
  GL_WIN_swap_hint                    = 1;

// EXT_vertex_array
  GL_VERTEX_ARRAY_EXT               = $8074;
  GL_NORMAL_ARRAY_EXT               = $8075;
  GL_COLOR_ARRAY_EXT                = $8076;
  GL_INDEX_ARRAY_EXT                = $8077;
  GL_TEXTURE_COORD_ARRAY_EXT        = $8078;
  GL_EDGE_FLAG_ARRAY_EXT            = $8079;
  GL_VERTEX_ARRAY_SIZE_EXT          = $807A;
  GL_VERTEX_ARRAY_TYPE_EXT          = $807B;
  GL_VERTEX_ARRAY_STRIDE_EXT        = $807C;
  GL_VERTEX_ARRAY_COUNT_EXT         = $807D;
  GL_NORMAL_ARRAY_TYPE_EXT          = $807E;
  GL_NORMAL_ARRAY_STRIDE_EXT        = $807F;
  GL_NORMAL_ARRAY_COUNT_EXT         = $8080;
  GL_COLOR_ARRAY_SIZE_EXT           = $8081;
  GL_COLOR_ARRAY_TYPE_EXT           = $8082;
  GL_COLOR_ARRAY_STRIDE_EXT         = $8083;
  GL_COLOR_ARRAY_COUNT_EXT          = $8084;
  GL_INDEX_ARRAY_TYPE_EXT           = $8085;
  GL_INDEX_ARRAY_STRIDE_EXT         = $8086;
  GL_INDEX_ARRAY_COUNT_EXT          = $8087;
  GL_TEXTURE_COORD_ARRAY_SIZE_EXT   = $8088;
  GL_TEXTURE_COORD_ARRAY_TYPE_EXT   = $8089;
  GL_TEXTURE_COORD_ARRAY_STRIDE_EXT = $808A;
  GL_TEXTURE_COORD_ARRAY_COUNT_EXT  = $808B;
  GL_EDGE_FLAG_ARRAY_STRIDE_EXT     = $808C;
  GL_EDGE_FLAG_ARRAY_COUNT_EXT      = $808D;
  GL_VERTEX_ARRAY_POINTER_EXT       = $808E;
  GL_NORMAL_ARRAY_POINTER_EXT       = $808F;
  GL_COLOR_ARRAY_POINTER_EXT        = $8090;
  GL_INDEX_ARRAY_POINTER_EXT        = $8091;
  GL_TEXTURE_COORD_ARRAY_POINTER_EXT = $8092;
  GL_EDGE_FLAG_ARRAY_POINTER_EXT    = $8093;


type
  PPointFloat = ^TPointFloat;
  TPointFloat = record
    X,Y: Single;
  end;

  PGlyphMetricsFloat = ^TGlyphMetricsFloat;
  TGlyphMetricsFloat = record
    gmfBlackBoxX: Single;
    gmfBlackBoxY: Single;
    gmfptGlyphOrigin: TPointFloat;
    gmfCellIncX: Single;
    gmfCellIncY: Single;
  end;

const
  WGL_FONT_LINES      = 0;
  WGL_FONT_POLYGONS   = 1;

{***********************************************************}

procedure glAccum (op: GLenum; value: GLfloat); stdcall;
procedure glAlphaFunc (func: GLenum; ref: GLclampf); stdcall;
procedure glBegin (mode: GLenum); stdcall;
procedure glBitmap (width, height: GLsizei; xorig, yorig: GLfloat;
                    xmove, ymove: GLfloat; bitmap: Pointer); stdcall;
procedure glBlendFunc (sfactor, dfactor: GLenum); stdcall;
procedure glCallList (list: GLuint); stdcall;
procedure glCallLists (n: GLsizei; cltype: GLenum; lists: Pointer); stdcall;
procedure glClear (mask: GLbitfield); stdcall;
procedure glClearAccum (red, green, blue, alpha: GLfloat); stdcall;
procedure glClearColor (red, green, blue, alpha: GLclampf); stdcall;
procedure glClearDepth (depth: GLclampd); stdcall;
procedure glClearIndex (c: GLfloat); stdcall;
procedure glClearStencil (s: GLint); stdcall;
procedure glClipPlane (plane: GLenum; equation: PGLDouble); stdcall;
procedure glColor3b (red, green, blue: GLbyte); stdcall;
procedure glColor3bv (v: PGLByte); stdcall;
procedure glColor3d (red, green, blue: GLdouble); stdcall;
procedure glColor3dv (v: PGLdouble); stdcall;
procedure glColor3f (red, green, blue: GLfloat); stdcall;
procedure glColor3fv (v: PGLfloat); stdcall;
procedure glColor3i (red, green, blue: GLint); stdcall;
procedure glColor3iv (v: PGLint); stdcall;
procedure glColor3s (red, green, blue: GLshort); stdcall;
procedure glColor3sv (v: PGLshort); stdcall;
procedure glColor3ub (red, green, blue: GLubyte); stdcall;
procedure glColor3ubv (v: PGLubyte); stdcall;
procedure glColor3ui (red, green, blue: GLuint); stdcall;
procedure glColor3uiv (v: PGLuint); stdcall;
procedure glColor3us (red, green, blue: GLushort); stdcall;
procedure glColor3usv (v: PGLushort); stdcall;
procedure glColor4b (red, green, blue, alpha: GLbyte); stdcall;
procedure glColor4bv (v: PGLbyte); stdcall;
procedure glColor4d (red, green, blue, alpha: GLdouble); stdcall;
procedure glColor4dv (v: PGLdouble); stdcall;
procedure glColor4f (red, green, blue, alpha: GLfloat); stdcall;
procedure glColor4fv (v: PGLfloat); stdcall;
procedure glColor4i (red, green, blue, alpha: GLint); stdcall;
procedure glColor4iv (v: PGLint); stdcall;
procedure glColor4s (red, green, blue, alpha: GLshort); stdcall;
procedure glColor4sv (v: PGLshort); stdcall;
procedure glColor4ub (red, green, blue, alpha: GLubyte); stdcall;
procedure glColor4ubv (v: PGLubyte); stdcall;
procedure glColor4ui (red, green, blue, alpha: GLuint); stdcall;
procedure glColor4uiv (v: PGLuint); stdcall;
procedure glColor4us (red, green, blue, alpha: GLushort); stdcall;
procedure glColor4usv (v: PGLushort); stdcall;
procedure glColorMask (red, green, blue, alpha: GLboolean); stdcall;
procedure glColorMaterial (face, mode: GLenum); stdcall;
procedure glCopyPixels (x,y: GLint; width, height: GLsizei; pixeltype: GLenum); stdcall;
procedure glCullFace (mode: GLenum); stdcall;
procedure glDeleteLists (list: GLuint; range: GLsizei); stdcall;
procedure glDepthFunc (func: GLenum); stdcall;
procedure glDepthMask (flag: GLboolean); stdcall;
procedure glDepthRange (zNear, zFar: GLclampd); stdcall;
procedure glDisable (cap: GLenum); stdcall;
procedure glDrawBuffer (mode: GLenum); stdcall;
procedure glDrawPixels (width, height: GLsizei; format, pixeltype: GLenum;
             pixels: Pointer); stdcall;
procedure glEdgeFlag (flag: GLboolean); stdcall;
procedure glEdgeFlagv (flag: PGLboolean); stdcall;
procedure glEnable (cap: GLenum); stdcall;
procedure glEnd; stdcall;
procedure glEndList; stdcall;
procedure glEvalCoord1d (u: GLdouble); stdcall;
procedure glEvalCoord1dv (u: PGLdouble); stdcall;
procedure glEvalCoord1f (u: GLfloat); stdcall;
procedure glEvalCoord1fv (u: PGLfloat); stdcall;
procedure glEvalCoord2d (u,v: GLdouble); stdcall;
procedure glEvalCoord2dv (u: PGLdouble); stdcall;
procedure glEvalCoord2f (u,v: GLfloat); stdcall;
procedure glEvalCoord2fv (u: PGLfloat); stdcall;
procedure glEvalMesh1 (mode: GLenum; i1, i2: GLint); stdcall;
procedure glEvalMesh2 (mode: GLenum; i1, i2, j1, j2: GLint); stdcall;
procedure glEvalPoint1 (i: GLint); stdcall;
procedure glEvalPoint2 (i,j: GLint); stdcall;
procedure glFeedbackBuffer (size: GLsizei; buftype: GLenum; buffer: PGLFloat); stdcall;
procedure glFinish; stdcall;
procedure glFlush; stdcall;
procedure glFogf (pname: GLenum; param: GLfloat); stdcall;
procedure glFogfv (pname: GLenum; params: PGLfloat); stdcall;
procedure glFogi (pname: GLenum; param: GLint); stdcall;
procedure glFogiv (pname: GLenum; params: PGLint); stdcall;
procedure glFrontFace (mode: GLenum); stdcall;
procedure glFrustum (left, right, bottom, top, zNear, zFar: GLdouble); stdcall;
function  glGenLists (range: GLsizei): GLuint; stdcall;
procedure glGetBooleanv (pname: GLenum; params: PGLboolean); stdcall;
procedure glGetClipPlane (plane: GLenum; equation: PGLdouble); stdcall;
procedure glGetDoublev (pname: GLenum; params: PGLdouble); stdcall;
function  glGetError: GLenum; stdcall;
procedure glGetFloatv (pname: GLenum; params: PGLfloat); stdcall;
procedure glGetIntegerv (pname: GLenum; params: PGLint); stdcall;
procedure glGetLightfv (light: GLenum; pname: GLenum; params: PGLfloat); stdcall;
procedure glGetLightiv (light: GLenum; pname: GLenum; params: PGLint); stdcall;
procedure glGetMapdv (target: GLenum; query: GLenum; v: PGLdouble); stdcall;
procedure glGetMapfv (target: GLenum; query: GLenum; v: PGLfloat); stdcall;
procedure glGetMapiv (target: GLenum; query: GLenum; v: PGLint); stdcall;
procedure glGetMaterialfv (face: GLenum; pname: GLenum; params: PGLfloat); stdcall;
procedure glGetMaterialiv (face: GLenum; pname: GLenum; params: PGLint); stdcall;
procedure glGetPixelMapfv (map: GLenum; values: PGLfloat); stdcall;
procedure glGetPixelMapuiv (map: GLenum; values: PGLuint); stdcall;
procedure glGetPixelMapusv (map: GLenum; values: PGLushort); stdcall;
procedure glGetPolygonStipple (var mask: GLubyte); stdcall;
function  glGetString (name: GLenum): PChar; stdcall;
procedure glGetTexEnvfv (target: GLenum; pname: GLenum; params: PGLfloat); stdcall;
procedure glGetTexEnviv (target: GLenum; pname: GLenum; params: PGLint); stdcall;
procedure glGetTexGendv (coord: GLenum; pname: GLenum; params: PGLdouble); stdcall;
procedure glGetTexGenfv (coord: GLenum; pname: GLenum; params: PGLfloat); stdcall;
procedure glGetTexGeniv (coord: GLenum; pname: GLenum; params: PGLint); stdcall;
procedure glGetTexImage (target: GLenum; level: GLint; format: GLenum; _type: GLenum; pixels: pointer); stdcall;
procedure glGetTexLevelParameterfv (target: GLenum; level: GLint; pname: GLenum; params: PGLfloat); stdcall;
procedure glGetTexLevelParameteriv (target: GLenum; level: GLint; pname: GLenum; params: PGLint); stdcall;
procedure glGetTexParameterfv (target, pname: GLenum; params: PGLfloat); stdcall;
procedure glGetTexParameteriv (target, pname: GLenum; params: PGLint); stdcall;
procedure glHint (target, mode: GLenum); stdcall;
procedure glIndexMask (mask: GLuint); stdcall;
procedure glIndexd (c: GLdouble); stdcall;
procedure glIndexdv (c: PGLdouble); stdcall;
procedure glIndexf (c: GLfloat); stdcall;
procedure glIndexfv (c: PGLfloat); stdcall;
procedure glIndexi (c: GLint); stdcall;
procedure glIndexiv (c: PGLint); stdcall;
procedure glIndexs (c: GLshort); stdcall;
procedure glIndexsv (c: PGLshort); stdcall;
procedure glInitNames; stdcall;
function  glIsEnabled (cap: GLenum): GLBoolean; stdcall;
function  glIsList (list: GLuint): GLBoolean;   stdcall;
procedure glLightModelf (pname: GLenum; param: GLfloat); stdcall;
procedure glLightModelfv (pname: GLenum; params: PGLfloat); stdcall;
procedure glLightModeli (pname: GLenum; param: GLint); stdcall;
procedure glLightModeliv (pname: GLenum; params: PGLint); stdcall;
procedure glLightf (light, pname: GLenum; param: GLfloat); stdcall;
procedure glLightfv (light, pname: GLenum; params: PGLfloat); stdcall;
procedure glLighti (light, pname: GLenum; param: GLint); stdcall;
procedure glLightiv (light, pname: GLenum; params: PGLint); stdcall;
procedure glLineStipple (factor: GLint; pattern: GLushort); stdcall;
procedure glLineWidth (width: GLfloat); stdcall;
procedure glListBase (base: GLuint); stdcall;
procedure glLoadIdentity; stdcall;
procedure glLoadMatrixd (m: PGLdouble); stdcall;
procedure glLoadMatrixf (m: PGLfloat); stdcall;
procedure glLoadName (name: GLuint); stdcall;
procedure glLogicOp (opcode: GLenum); stdcall;
procedure glMap1d (target: GLenum; u1,u2: GLdouble; stride, order: GLint;
  Points: PGLdouble); stdcall;
procedure glMap1f (target: GLenum; u1,u2: GLfloat; stride, order: GLint;
  Points: PGLfloat); stdcall;
procedure glMap2d (target: GLenum;
  u1,u2: GLdouble; ustride, uorder: GLint;
  v1,v2: GLdouble; vstride, vorder: GLint; Points: PGLdouble); stdcall;
procedure glMap2f (target: GLenum;
  u1,u2: GLfloat; ustride, uorder: GLint;
  v1,v2: GLfloat; vstride, vorder: GLint; Points: PGLfloat); stdcall;
procedure glMapGrid1d (un: GLint; u1, u2: GLdouble); stdcall;
procedure glMapGrid1f (un: GLint; u1, u2: GLfloat); stdcall;
procedure glMapGrid2d (un: GLint; u1, u2: GLdouble;
                       vn: GLint; v1, v2: GLdouble); stdcall;
procedure glMapGrid2f (un: GLint; u1, u2: GLfloat;
                       vn: GLint; v1, v2: GLfloat); stdcall;
procedure glMaterialf (face, pname: GLenum; param: GLfloat); stdcall;
procedure glMaterialfv (face, pname: GLenum; params: PGLfloat); stdcall;
procedure glMateriali (face, pname: GLenum; param: GLint); stdcall;
procedure glMaterialiv (face, pname: GLenum; params: PGLint); stdcall;
procedure glMatrixMode (mode: GLenum); stdcall;
procedure glMultMatrixd (m: PGLdouble); stdcall;
procedure glMultMatrixf (m: PGLfloat); stdcall;
procedure glNewList (ListIndex: GLuint; mode: GLenum); stdcall;
procedure glNormal3b (nx, ny, nz: GLbyte); stdcall;
procedure glNormal3bv (v: PGLbyte); stdcall;
procedure glNormal3d (nx, ny, nz: GLdouble); stdcall;
procedure glNormal3dv (v: PGLdouble); stdcall;
procedure glNormal3f (nx, ny, nz: GLFloat); stdcall;
procedure glNormal3fv (v: PGLfloat); stdcall;
procedure glNormal3i (nx, ny, nz: GLint); stdcall;
procedure glNormal3iv (v: PGLint); stdcall;
procedure glNormal3s (nx, ny, nz: GLshort); stdcall;
procedure glNormal3sv (v: PGLshort); stdcall;
procedure glOrtho (left, right, bottom, top, zNear, zFar: GLdouble); stdcall;
procedure glPassThrough (token: GLfloat); stdcall;
procedure glPixelMapfv (map: GLenum; mapsize: GLint; values: PGLfloat); stdcall;
procedure glPixelMapuiv (map: GLenum; mapsize: GLint; values: PGLuint); stdcall;
procedure glPixelMapusv (map: GLenum; mapsize: GLint; values: PGLushort); stdcall;
procedure glPixelStoref (pname: GLenum; param: GLfloat); stdcall;
procedure glPixelStorei (pname: GLenum; param: GLint); stdcall;
procedure glPixelTransferf (pname: GLenum; param: GLfloat); stdcall;
procedure glPixelTransferi (pname: GLenum; param: GLint); stdcall;
procedure glPixelZoom (xfactor, yfactor: GLfloat); stdcall;
procedure glPointSize (size: GLfloat); stdcall;
procedure glPolygonMode (face, mode: GLenum); stdcall;
procedure glPolygonStipple (mask: PGLubyte); stdcall;
procedure glPopAttrib; stdcall;
procedure glPopMatrix; stdcall;
procedure glPopName; stdcall;
procedure glPushAttrib(mask: GLbitfield); stdcall;
procedure glPushMatrix; stdcall;
procedure glPushName(name: GLuint); stdcall;
procedure glRasterPos2d (x,y: GLdouble); stdcall;
procedure glRasterPos2dv (v: PGLdouble); stdcall;
procedure glRasterPos2f (x,y: GLfloat); stdcall;
procedure glRasterPos2fv (v: PGLfloat); stdcall;
procedure glRasterPos2i (x,y: GLint); stdcall;
procedure glRasterPos2iv (v: PGLint); stdcall;
procedure glRasterPos2s (x,y: GLshort); stdcall;
procedure glRasterPos2sv (v: PGLshort); stdcall;
procedure glRasterPos3d (x,y,z: GLdouble); stdcall;
procedure glRasterPos3dv (v: PGLdouble); stdcall;
procedure glRasterPos3f (x,y,z: GLfloat); stdcall;
procedure glRasterPos3fv (v: PGLfloat); stdcall;
procedure glRasterPos3i (x,y,z: GLint); stdcall;
procedure glRasterPos3iv (v: PGLint); stdcall;
procedure glRasterPos3s (x,y,z: GLshort); stdcall;
procedure glRasterPos3sv (v: PGLshort); stdcall;
procedure glRasterPos4d (x,y,z,w: GLdouble); stdcall;
procedure glRasterPos4dv (v: PGLdouble); stdcall;
procedure glRasterPos4f (x,y,z: GLfloat); stdcall;
procedure glRasterPos4fv (v: PGLfloat); stdcall;
procedure glRasterPos4i (x,y,z: GLint); stdcall;
procedure glRasterPos4iv (v: PGLint); stdcall;
procedure glRasterPos4s (x,y,z: GLshort); stdcall;
procedure glRasterPos4sv (v: PGLshort); stdcall;
procedure glReadBuffer (mode: GLenum); stdcall;
procedure glReadPixels (x,y: GLint; width, height: GLsizei;
  format, _type: GLenum; pixels: Pointer); stdcall;
procedure glRectd (x1, y1, x2, y2: GLdouble); stdcall;
procedure glRectdv (v1, v2: PGLdouble); stdcall;
procedure glRectf (x1, y1, x2, y2: GLfloat); stdcall;
procedure glRectfv (v1, v2: PGLfloat); stdcall;
procedure glRecti (x1, y1, x2, y2: GLint); stdcall;
procedure glRectiv (v1, v2: PGLint); stdcall;
procedure glRects (x1, y1, x2, y2: GLshort); stdcall;
procedure glRectsv (v1, v2: PGLshort); stdcall;
function  glRenderMode (mode: GLenum): GLint; stdcall;
procedure glRotated (angle, x,y,z: GLdouble); stdcall;
procedure glRotatef (angle, x,y,z: GLfloat); stdcall;
procedure glScaled (x,y,z: GLdouble); stdcall;
procedure glScalef (x,y,z: GLfloat); stdcall;
procedure glScissor (x,y: GLint; width, height: GLsizei); stdcall;
procedure glSelectBuffer (size: GLsizei; buffer: PGLuint); stdcall;
procedure glShadeModel (mode: GLenum); stdcall;
procedure glStencilFunc (func: GLenum; ref: GLint; mask: GLuint); stdcall;
procedure glStencilMask (mask: GLuint); stdcall;
procedure glStencilOp (fail, zfail, zpass: GLenum); stdcall;
procedure glTexCoord1d (s: GLdouble); stdcall;
procedure glTexCoord1dv (v: PGLdouble); stdcall;
procedure glTexCoord1f (s: GLfloat); stdcall;
procedure glTexCoord1fv (v: PGLfloat); stdcall;
procedure glTexCoord1i (s: GLint); stdcall;
procedure glTexCoord1iv (v: PGLint); stdcall;
procedure glTexCoord1s (s: GLshort); stdcall;
procedure glTexCoord1sv (v: PGLshort); stdcall;
procedure glTexCoord2d (s,t: GLdouble); stdcall;
procedure glTexCoord2dv (v: PGLdouble); stdcall;
procedure glTexCoord2f (s,t: GLfloat); stdcall;
procedure glTexCoord2fv (v: PGLfloat); stdcall;
procedure glTexCoord2i (s,t: GLint); stdcall;
procedure glTexCoord2iv (v: PGLint); stdcall;
procedure glTexCoord2s (s,t: GLshort); stdcall;
procedure glTexCoord2sv (v: PGLshort); stdcall;
procedure glTexCoord3d (s,t,r: GLdouble); stdcall;
procedure glTexCoord3dv (v: PGLdouble); stdcall;
procedure glTexCoord3f (s,t,r: GLfloat); stdcall;
procedure glTexCoord3fv (v: PGLfloat); stdcall;
procedure glTexCoord3i (s,t,r: GLint); stdcall;
procedure glTexCoord3iv (v: PGLint); stdcall;
procedure glTexCoord3s (s,t,r: GLshort); stdcall;
procedure glTexCoord3sv (v: PGLshort); stdcall;
procedure glTexCoord4d (s,t,r,q: GLdouble); stdcall;
procedure glTexCoord4dv (v: PGLdouble); stdcall;
procedure glTexCoord4f (s,t,r: GLfloat); stdcall;
procedure glTexCoord4fv (v: PGLfloat); stdcall;
procedure glTexCoord4i (s,t,r: GLint); stdcall;
procedure glTexCoord4iv (v: PGLint); stdcall;
procedure glTexCoord4s (s,t,r: GLshort); stdcall;
procedure glTexCoord4sv (v: PGLshort); stdcall;
procedure glTexEnvf (target, pname: GLenum; param: GLfloat); stdcall;
procedure glTexEnvfv (target, pname: GLenum; params: PGLfloat); stdcall;
procedure glTexEnvi (target, pname: GLenum; param: GLint); stdcall;
procedure glTexEnviv (target, pname: GLenum; params: PGLint); stdcall;
procedure glTexGend (coord, pname: GLenum; param: GLdouble); stdcall;
procedure glTexGendv (coord, pname: GLenum; params: PGLdouble); stdcall;
procedure glTexGenf (coord, pname: GLenum; param: GLfloat); stdcall;
procedure glTexGenfv (coord, pname: GLenum; params: PGLfloat); stdcall;
procedure glTexGeni (coord, pname: GLenum; param: GLint); stdcall;
procedure glTexGeniv (coord, pname: GLenum; params: PGLint); stdcall;
procedure glTexImage1D (target: GLenum; level, components: GLint;
  width: GLsizei; border: GLint; format, _type: GLenum; pixels: Pointer); stdcall;
procedure glTexImage2D (target: GLenum; level, components: GLint;
  width, height: GLsizei; border: GLint; format, _type: GLenum; pixels: Pointer); stdcall;
procedure glTexParameterf (target, pname: GLenum; param: GLfloat); stdcall;
procedure glTexParameterfv (target, pname: GLenum; params: PGLfloat); stdcall;
procedure glTexParameteri (target, pname: GLenum; param: GLint); stdcall;
procedure glTexParameteriv (target, pname: GLenum; params: PGLint); stdcall;
procedure glTranslated (x,y,z: GLdouble); stdcall;
procedure glTranslatef (x,y,z: GLfloat); stdcall;
procedure glVertex2d (x,y: GLdouble); stdcall;
procedure glVertex2dv (v: PGLdouble); stdcall;
procedure glVertex2f (x,y: GLfloat); stdcall;
procedure glVertex2fv (v: PGLfloat); stdcall;
procedure glVertex2i (x,y: GLint); stdcall;
procedure glVertex2iv (v: PGLint); stdcall;
procedure glVertex2s (x,y: GLshort); stdcall;
procedure glVertex2sv (v: PGLshort); stdcall;
procedure glVertex3d (x,y,z: GLdouble); stdcall;
procedure glVertex3dv (v: PGLdouble); stdcall;
procedure glVertex3f (x,y,z: GLfloat); stdcall;
procedure glVertex3fv (v: PGLfloat); stdcall;
procedure glVertex3i (x,y,z: GLint); stdcall;
procedure glVertex3iv (v: PGLint); stdcall;
procedure glVertex3s (x,y,z: GLshort); stdcall;
procedure glVertex3sv (v: PGLshort); stdcall;
procedure glVertex4d (x,y,z,w: GLdouble); stdcall;
procedure glVertex4dv (v: PGLdouble); stdcall;
procedure glVertex4f (x,y,z,w: GLfloat); stdcall;
procedure glVertex4fv (v: PGLfloat); stdcall;
procedure glVertex4i (x,y,z,w: GLint); stdcall;
procedure glVertex4iv (v: PGLint); stdcall;
procedure glVertex4s (x,y,z,w: GLshort); stdcall;
procedure glVertex4sv (v: PGLshort); stdcall;
procedure glViewport (x,y: GLint; width, height: GLsizei); stdcall;

type

// EXT_vertex_array
  TGLARRAYELEMENTEXTPROC = procedure (i: GLint) stdcall;
  TGLDRAWARRAYSEXTPROC = procedure (mode: GLenum; first: GLint; count: GLsizei) stdcall;
  TGLVERTEXPOINTEREXTPROC = procedure (size: GLint; type_: GLenum;
    stride, count: GLsizei; P: Pointer) stdcall;
  TGLNORMALPOINTEREXTPROC = procedure (type_: GLenum; stride, count: GLsizei;
    P: Pointer) stdcall;
  TGLCOLORPOINTEREXTPROC = procedure (size: GLint; type_: GLenum;
    stride, count: GLsizei; P: Pointer) stdcall;
  TGLINDEXPOINTEREXTPROC = procedure (type_: GLenum; stride, count: GLsizei;
    P: Pointer) stdcall;
  TGLTEXCOORDPOINTEREXTPROC = procedure (size: GLint; type_: GLenum;
    stride, count: GLsizei; P: Pointer) stdcall;
  TGLEDGEFLAGPOINTEREXTPROC = procedure (stride, count: GLsizei;
    P: PGLboolean) stdcall;
  TGLGETPOINTERVEXTPROC = procedure (pname: GLenum; var Params) stdcall;

// WIN_swap_hint

  TGLADDSWAPHINTRECTWINPROC = procedure (x, y: GLint; width, height: GLsizei) stdcall;

{ OpenGL Utility routines (glu.h) =======================================}

function gluErrorString (errCode: GLenum): PChar; stdcall;
function gluErrorUnicodeStringEXT (errCode: GLenum): PWChar; stdcall;
function gluGetString (name: GLenum): PChar; stdcall;

procedure gluLookAt(eyex, eyey, eyez,
                    centerx, centery, centerz,
                    upx, upy, upz: GLdouble); stdcall;
procedure gluOrtho2D(left, right, bottom, top: GLdouble); stdcall;
procedure gluPerspective(fovy, aspect, zNear, zFar: GLdouble); stdcall;
procedure gluPickMatrix (x, y, width, height: GLdouble; viewport: PGLint); stdcall;
function  gluProject (objx, objy, obyz: GLdouble;
                      modelMatrix: PGLdouble;
                      projMatrix: PGLdouble;
                      viewport: PGLint;
                      var winx, winy, winz: GLDouble): Integer; stdcall;
function  gluUnProject(winx, winy, winz: GLdouble;
                      modelMatrix: PGLdouble;
                      projMatrix: PGLdouble;
                      viewport: PGLint;
                      var objx, objy, objz: GLdouble): Integer; stdcall;
function  gluScaleImage(format: GLenum;
   widthin, heightin: GLint; typein: GLenum; datain: Pointer;
   widthout, heightout: GLint; typeout: GLenum; dataout: Pointer): Integer; stdcall;

function  gluBuild1DMipmaps (target: GLenum; components, width: GLint;
                             format, atype: GLenum; data: Pointer): Integer; stdcall;
function  gluBuild2DMipmaps (target: GLenum; components, width: GLint;
                             format, atype: GLenum; data: Pointer): Integer; stdcall;

type
  _GLUquadricObj = record end;
  GLUquadricObj = ^_GLUquadricObj;

  TGLUquadricErrorProc = procedure (error: GLenum) stdcall;

function  gluNewQuadric: GLUquadricObj; stdcall;
procedure gluDeleteQuadric (state: GLUquadricObj); stdcall;
procedure gluQuadricNormals (quadObject: GLUquadricObj; normals: GLenum);  stdcall;
procedure gluQuadricTexture (quadObject: GLUquadricObj; textureCoords: GLboolean );stdcall;
procedure gluQuadricOrientation (quadObject: GLUquadricObj; orientation: GLenum); stdcall;
procedure gluQuadricDrawStyle (quadObject: GLUquadricObj; drawStyle: GLenum); stdcall;
procedure gluCylinder (quadObject: GLUquadricObj;
  baseRadius, topRadius, height: GLdouble; slices, stacks: GLint); stdcall;
procedure gluDisk (quadObject: GLUquadricObj;
  innerRadius, outerRadius: GLdouble; slices, loops: GLint); stdcall;
procedure gluPartialDisk (quadObject: GLUquadricObj;
  innerRadius, outerRadius: GLdouble; slices, loops: GLint;
  startAngle, sweepAngle: GLdouble); stdcall;
procedure gluSphere (quadObject: GLUquadricObj; radius: GLdouble; slices, loops: GLint); stdcall;
procedure gluQuadricCallback (quadObject: GLUquadricObj; which: GLenum;
  callback: Pointer); stdcall;

type
  _GLUtesselator = record end;
  GLUtesselator = ^_GLUtesselator;

  // tesselator callback procedure types
  TGLUtessBeginProc = procedure (a: GLenum) stdcall;
  TGLUtessEdgeFlagProc = procedure (flag: GLboolean) stdcall;
  TGLUtessVertexProc = procedure (p: Pointer) stdcall;
  TGLUtessEndProc = procedure stdcall;
  TGLUtessErrorProc = TGLUquadricErrorProc;
  TGLUtessCombineProc = procedure (a: PGLdouble; b: Pointer;
                                   c: PGLfloat; var d: Pointer) stdcall;

function gluNewTess: GLUtesselator; stdcall;
procedure gluDeleteTess( tess: GLUtesselator ); stdcall;
procedure gluTessBeginPolygon( tess: GLUtesselator ); stdcall;
procedure gluTessBeginContour( tess: GLUtesselator ); stdcall;
procedure gluTessVertex( tess: GLUtesselator; coords: PGLdouble; data: Pointer ); stdcall;
procedure gluTessEndContour( tess: GLUtesselator ); stdcall;
procedure gluTessEndPolygon( tess: GLUtesselator ); stdcall;
procedure gluTessProperty( tess: GLUtesselator; which: GLenum; value: GLdouble); stdcall;
procedure gluTessNormal( tess: GLUtesselator; x,y,z: GLdouble); stdcall;
procedure gluTessCallback( tess: GLUtesselator; which: GLenum; callback: pointer); stdcall;

type
  TGLUnurbsObj = record end;
  GLUnurbsObj = ^TGLUnurbsObj;

  TGLUnurbsErrorProc = TGLUquadricErrorProc;

function gluNewNurbsRenderer: GLUnurbsObj; stdcall;
procedure gluDeleteNurbsRenderer (nobj: GLUnurbsObj); stdcall;
procedure gluBeginSurface (nobj: GLUnurbsObj); stdcall;
procedure gluBeginCurve (nobj: GLUnurbsObj); stdcall;
procedure gluEndCurve (nobj: GLUnurbsObj); stdcall;
procedure gluEndSurface (nobj: GLUnurbsObj); stdcall;
procedure gluBeginTrim (nobj: GLUnurbsObj); stdcall;
procedure gluEndTrim (nobj: GLUnurbsObj); stdcall;
procedure gluPwlCurve (nobj: GLUnurbsObj; count: GLint; points: PGLfloat;
  stride: GLint; _type: GLenum); stdcall;
procedure gluNurbsCurve (nobj: GLUnurbsObj; nknots: GLint; knot: PGLfloat;
  stride: GLint; ctlpts: PGLfloat; order: GLint; _type: GLenum); stdcall;
procedure gluNurbsSurface (nobj: GLUnurbsObj;
  sknot_count: GLint; sknot: PGLfloat;
  tknot_count: GLint; tknot: PGLfloat;
  s_stride, t_stride: GLint;
  ctlpts: PGLfloat; sorder, torder: GLint; _type: GLenum); stdcall;
procedure gluLoadSamplingMatrices (nobj: GLUnurbsObj;
  modelMatrix: PGLdouble; projMatrix: PGLdouble; viewport: PGLint); stdcall;
procedure gluNurbsProperty (nobj: GLUnurbsObj; prop: GLenum; value: GLfloat); stdcall;
procedure gluGetNurbsProperty (nobj: GLUnurbsObj; prop: GLenum; var value: GLfloat); stdcall;
procedure gluNurbsCallback (nobj: GLUnurbsObj; which: GLenum; callback: pointer); stdcall;


{****           Generic constants               ****}
const
  GLU_VERSION_1_1  =               1;

{ Errors: (return value 0 = no error) }
  GLU_INVALID_ENUM       = 100900;
  GLU_INVALID_VALUE      = 100901;
  GLU_OUT_OF_MEMORY      = 100902;
  GLU_INCOMPATIBLE_GL_VERSION  =   100903;

{ gets }
  GLU_VERSION            = 100800;
  GLU_EXTENSIONS         = 100801;

{ For laughs: }
  GLU_TRUE               = GL_TRUE;
  GLU_FALSE              = GL_FALSE;


{***           Quadric constants               ***}

{ Types of normals: }
  GLU_SMOOTH             = 100000;
  GLU_FLAT               = 100001;
  GLU_NONE               = 100002;

{ DrawStyle types: }
  GLU_POINT              = 100010;
  GLU_LINE               = 100011;
  GLU_FILL               = 100012;
  GLU_SILHOUETTE         = 100013;

{ Orientation types: }
  GLU_OUTSIDE            = 100020;
  GLU_INSIDE             = 100021;

{ Callback types: }
{      GLU_ERROR               100103 }


{***           Tesselation constants           ***}

  GLU_TESS_MAX_COORD     =         1.0e150;

{ Property types: }
  GLU_TESS_WINDING_RULE  =         100110;
  GLU_TESS_BOUNDARY_ONLY =         100111;
  GLU_TESS_TOLERANCE     =         100112;

{ Possible winding rules: }
  GLU_TESS_WINDING_ODD          =  100130;
  GLU_TESS_WINDING_NONZERO      =  100131;
  GLU_TESS_WINDING_POSITIVE     =  100132;
  GLU_TESS_WINDING_NEGATIVE     =  100133;
  GLU_TESS_WINDING_ABS_GEQ_TWO  =  100134;

{ Callback types: }
  GLU_TESS_BEGIN     = 100100 ;     { void (*)(GLenum    type)         }
  GLU_TESS_VERTEX    = 100101 ;     { void (*)(void      *data)        }
  GLU_TESS_END       = 100102 ;     { void (*)(void)                   }
  GLU_TESS_ERROR     = 100103 ;     { void (*)(GLenum    errno)        }
  GLU_TESS_EDGE_FLAG = 100104 ;     { void (*)(GLboolean boundaryEdge) }
  GLU_TESS_COMBINE   = 100105 ;     { void (*)(GLdouble  coords[3],;
                                                    void      *data[4],;
                                                    GLfloat   weight[4],;
                                                    void      **dataOut)    }

{ Errors: }
  GLU_TESS_ERROR1    = 100151;
  GLU_TESS_ERROR2    = 100152;
  GLU_TESS_ERROR3    = 100153;
  GLU_TESS_ERROR4    = 100154;
  GLU_TESS_ERROR5    = 100155;
  GLU_TESS_ERROR6    = 100156;
  GLU_TESS_ERROR7    = 100157;
  GLU_TESS_ERROR8    = 100158;

  GLU_TESS_MISSING_BEGIN_POLYGON  = GLU_TESS_ERROR1;
  GLU_TESS_MISSING_BEGIN_CONTOUR  = GLU_TESS_ERROR2;
  GLU_TESS_MISSING_END_POLYGON    = GLU_TESS_ERROR3;
  GLU_TESS_MISSING_END_CONTOUR    = GLU_TESS_ERROR4;
  GLU_TESS_COORD_TOO_LARGE        = GLU_TESS_ERROR5;
  GLU_TESS_NEED_COMBINE_CALLBACK  = GLU_TESS_ERROR6;

{***           NURBS constants                 ***}

{ Properties: }
  GLU_AUTO_LOAD_MATRIX          =  100200;
  GLU_CULLING                   =  100201;
  GLU_SAMPLING_TOLERANCE        =  100203;
  GLU_DISPLAY_MODE              =  100204;
  GLU_PARAMETRIC_TOLERANCE      =  100202;
  GLU_SAMPLING_METHOD           =  100205;
  GLU_U_STEP                    =  100206;
  GLU_V_STEP                    =  100207;

{ Sampling methods: }
  GLU_PATH_LENGTH               =  100215;
  GLU_PARAMETRIC_ERROR          =  100216;
  GLU_DOMAIN_DISTANCE           =  100217;

{ Trimming curve types }
  GLU_MAP1_TRIM_2       =  100210;
  GLU_MAP1_TRIM_3       =  100211;

{ Display modes: }
{      GLU_FILL                100012 }
  GLU_OUTLINE_POLYGON    = 100240;
  GLU_OUTLINE_PATCH      = 100241;

{ Callbacks: }
{      GLU_ERROR               100103 }

{ Errors: }
  GLU_NURBS_ERROR1       = 100251;
  GLU_NURBS_ERROR2       = 100252;
  GLU_NURBS_ERROR3       = 100253;
  GLU_NURBS_ERROR4       = 100254;
  GLU_NURBS_ERROR5       = 100255;
  GLU_NURBS_ERROR6       = 100256;
  GLU_NURBS_ERROR7       = 100257;
  GLU_NURBS_ERROR8       = 100258;
  GLU_NURBS_ERROR9       = 100259;
  GLU_NURBS_ERROR10      = 100260;
  GLU_NURBS_ERROR11      = 100261;
  GLU_NURBS_ERROR12      = 100262;
  GLU_NURBS_ERROR13      = 100263;
  GLU_NURBS_ERROR14      = 100264;
  GLU_NURBS_ERROR15      = 100265;
  GLU_NURBS_ERROR16      = 100266;
  GLU_NURBS_ERROR17      = 100267;
  GLU_NURBS_ERROR18      = 100268;
  GLU_NURBS_ERROR19      = 100269;
  GLU_NURBS_ERROR20      = 100270;
  GLU_NURBS_ERROR21      = 100271;
  GLU_NURBS_ERROR22      = 100272;
  GLU_NURBS_ERROR23      = 100273;
  GLU_NURBS_ERROR24      = 100274;
  GLU_NURBS_ERROR25      = 100275;
  GLU_NURBS_ERROR26      = 100276;
  GLU_NURBS_ERROR27      = 100277;
  GLU_NURBS_ERROR28      = 100278;
  GLU_NURBS_ERROR29      = 100279;
  GLU_NURBS_ERROR30      = 100280;
  GLU_NURBS_ERROR31      = 100281;
  GLU_NURBS_ERROR32      = 100282;
  GLU_NURBS_ERROR33      = 100283;
  GLU_NURBS_ERROR34      = 100284;
  GLU_NURBS_ERROR35      = 100285;
  GLU_NURBS_ERROR36      = 100286;
  GLU_NURBS_ERROR37      = 100287;

{
/****           Backwards compatibility for old tesselator           ****/

typedef GLUtesselator GLUtriangulatorObj;

procedure   gluBeginPolygon( tess: GLUtesselator );

procedure   gluNextContour(  tess: GLUtesselator,
                                 GLenum        type );

procedure   gluEndPolygon(   tess: GLUtesselator );

/* Contours types -- obsolete! */
#define GLU_CW          100120
#define GLU_CCW         100121
#define GLU_INTERIOR    100122
#define GLU_EXTERIOR    100123
#define GLU_UNKNOWN     100124

/* Names without "TESS_" prefix */
#define GLU_BEGIN       GLU_TESS_BEGIN
#define GLU_VERTEX      GLU_TESS_VERTEX
#define GLU_END         GLU_TESS_END
#define GLU_ERROR       GLU_TESS_ERROR
#define GLU_EDGE_FLAG   GLU_TESS_EDGE_FLAG
}

{ GDI support routines for OpenGL ==========================================}

function wglGetProcAddress(ProcName: PChar): Pointer;  stdcall;

const
  glu32 = 'glu32.dll';

implementation

procedure glAccum; external opengl32;
procedure glAlphaFunc; external opengl32;
procedure glBegin; external opengl32;
procedure glBitmap; external opengl32;
procedure glBlendFunc; external opengl32;
procedure glCallList; external opengl32;
procedure glCallLists; external opengl32;
procedure glClear; external opengl32;
procedure glClearAccum; external opengl32;
procedure glClearColor; external opengl32;
procedure glClearDepth; external opengl32;
procedure glClearIndex; external opengl32;
procedure glClearStencil; external opengl32;
procedure glClipPlane; external opengl32;
procedure glColor3b; external opengl32;
procedure glColor3bv; external opengl32;
procedure glColor3d; external opengl32;
procedure glColor3dv; external opengl32;
procedure glColor3f; external opengl32;
procedure glColor3fv; external opengl32;
procedure glColor3i; external opengl32;
procedure glColor3iv; external opengl32;
procedure glColor3s; external opengl32;
procedure glColor3sv; external opengl32;
procedure glColor3ub; external opengl32;
procedure glColor3ubv; external opengl32;
procedure glColor3ui; external opengl32;
procedure glColor3uiv; external opengl32;
procedure glColor3us; external opengl32;
procedure glColor3usv; external opengl32;
procedure glColor4b; external opengl32;
procedure glColor4bv; external opengl32;
procedure glColor4d; external opengl32;
procedure glColor4dv; external opengl32;
procedure glColor4f; external opengl32;
procedure glColor4fv; external opengl32;
procedure glColor4i; external opengl32;
procedure glColor4iv; external opengl32;
procedure glColor4s; external opengl32;
procedure glColor4sv; external opengl32;
procedure glColor4ub; external opengl32;
procedure glColor4ubv; external opengl32;
procedure glColor4ui; external opengl32;
procedure glColor4uiv; external opengl32;
procedure glColor4us; external opengl32;
procedure glColor4usv; external opengl32;
procedure glColorMask; external opengl32;
procedure glColorMaterial; external opengl32;
procedure glCopyPixels; external opengl32;
procedure glCullFace; external opengl32;
procedure glDeleteLists; external opengl32;
procedure glDepthFunc; external opengl32;
procedure glDepthMask; external opengl32;
procedure glDepthRange; external opengl32;
procedure glDisable; external opengl32;
procedure glDrawBuffer; external opengl32;
procedure glDrawPixels; external opengl32;
procedure glEdgeFlag; external opengl32;
procedure glEdgeFlagv; external opengl32;
procedure glEnable; external opengl32;
procedure glEnd; external opengl32;
procedure glEndList; external opengl32;
procedure glEvalCoord1d; external opengl32;
procedure glEvalCoord1dv; external opengl32;
procedure glEvalCoord1f; external opengl32;
procedure glEvalCoord1fv; external opengl32;
procedure glEvalCoord2d; external opengl32;
procedure glEvalCoord2dv; external opengl32;
procedure glEvalCoord2f; external opengl32;
procedure glEvalCoord2fv; external opengl32;
procedure glEvalMesh1; external opengl32;
procedure glEvalMesh2; external opengl32;
procedure glEvalPoint1; external opengl32;
procedure glEvalPoint2; external opengl32;
procedure glFeedbackBuffer; external opengl32;
procedure glFinish; external opengl32;
procedure glFlush; external opengl32;
procedure glFogf; external opengl32;
procedure glFogfv; external opengl32;
procedure glFogi; external opengl32;
procedure glFogiv; external opengl32;
procedure glFrontFace; external opengl32;
procedure glFrustum; external opengl32;
function  glGenLists; external opengl32;
procedure glGetBooleanv; external opengl32;
procedure glGetClipPlane; external opengl32;
procedure glGetDoublev; external opengl32;
function  glGetError: GLenum; external opengl32;
procedure glGetFloatv; external opengl32;
procedure glGetIntegerv; external opengl32;
procedure glGetLightfv; external opengl32;
procedure glGetLightiv; external opengl32;
procedure glGetMapdv; external opengl32;
procedure glGetMapfv; external opengl32;
procedure glGetMapiv; external opengl32;
procedure glGetMaterialfv; external opengl32;
procedure glGetMaterialiv; external opengl32;
procedure glGetPixelMapfv; external opengl32;
procedure glGetPixelMapuiv; external opengl32;
procedure glGetPixelMapusv; external opengl32;
procedure glGetPolygonStipple; external opengl32;
function  glGetString; external opengl32;
procedure glGetTexEnvfv; external opengl32;
procedure glGetTexEnviv; external opengl32;
procedure glGetTexGendv; external opengl32;
procedure glGetTexGenfv; external opengl32;
procedure glGetTexGeniv; external opengl32;
procedure glGetTexImage; external opengl32;
procedure glGetTexLevelParameterfv; external opengl32;
procedure glGetTexLevelParameteriv; external opengl32;
procedure glGetTexParameterfv; external opengl32;
procedure glGetTexParameteriv; external opengl32;
procedure glHint; external opengl32;
procedure glIndexMask; external opengl32;
procedure glIndexd; external opengl32;
procedure glIndexdv; external opengl32;
procedure glIndexf; external opengl32;
procedure glIndexfv; external opengl32;
procedure glIndexi; external opengl32;
procedure glIndexiv; external opengl32;
procedure glIndexs; external opengl32;
procedure glIndexsv; external opengl32;
procedure glInitNames; external opengl32;
function  glIsEnabled; external opengl32;
function  glIsList; external opengl32;
procedure glLightModelf; external opengl32;
procedure glLightModelfv; external opengl32;
procedure glLightModeli; external opengl32;
procedure glLightModeliv; external opengl32;
procedure glLightf; external opengl32;
procedure glLightfv; external opengl32;
procedure glLighti; external opengl32;
procedure glLightiv; external opengl32;
procedure glLineStipple; external opengl32;
procedure glLineWidth; external opengl32;
procedure glListBase; external opengl32;
procedure glLoadIdentity; external opengl32;
procedure glLoadMatrixd; external opengl32;
procedure glLoadMatrixf; external opengl32;
procedure glLoadName; external opengl32;
procedure glLogicOp; external opengl32;
procedure glMap1d; external opengl32;
procedure glMap1f; external opengl32;
procedure glMap2d; external opengl32;
procedure glMap2f; external opengl32;
procedure glMapGrid1d; external opengl32;
procedure glMapGrid1f; external opengl32;
procedure glMapGrid2d; external opengl32;
procedure glMapGrid2f; external opengl32;
procedure glMaterialf; external opengl32;
procedure glMaterialfv; external opengl32;
procedure glMateriali; external opengl32;
procedure glMaterialiv; external opengl32;
procedure glMatrixMode; external opengl32;
procedure glMultMatrixd; external opengl32;
procedure glMultMatrixf; external opengl32;
procedure glNewList; external opengl32;
procedure glNormal3b; external opengl32;
procedure glNormal3bv; external opengl32;
procedure glNormal3d; external opengl32;
procedure glNormal3dv; external opengl32;
procedure glNormal3f; external opengl32;
procedure glNormal3fv; external opengl32;
procedure glNormal3i; external opengl32;
procedure glNormal3iv; external opengl32;
procedure glNormal3s; external opengl32;
procedure glNormal3sv; external opengl32;
procedure glOrtho; external opengl32;
procedure glPassThrough; external opengl32;
procedure glPixelMapfv; external opengl32;
procedure glPixelMapuiv; external opengl32;
procedure glPixelMapusv; external opengl32;
procedure glPixelStoref; external opengl32;
procedure glPixelStorei; external opengl32;
procedure glPixelTransferf; external opengl32;
procedure glPixelTransferi; external opengl32;
procedure glPixelZoom; external opengl32;
procedure glPointSize; external opengl32;
procedure glPolygonMode; external opengl32;
procedure glPolygonStipple; external opengl32;
procedure glPopAttrib; external opengl32;
procedure glPopMatrix; external opengl32;
procedure glPopName; external opengl32;
procedure glPushAttrib; external opengl32;
procedure glPushMatrix; external opengl32;
procedure glPushName; external opengl32;
procedure glRasterPos2d; external opengl32;
procedure glRasterPos2dv; external opengl32;
procedure glRasterPos2f; external opengl32;
procedure glRasterPos2fv; external opengl32;
procedure glRasterPos2i; external opengl32;
procedure glRasterPos2iv; external opengl32;
procedure glRasterPos2s; external opengl32;
procedure glRasterPos2sv; external opengl32;
procedure glRasterPos3d; external opengl32;
procedure glRasterPos3dv; external opengl32;
procedure glRasterPos3f; external opengl32;
procedure glRasterPos3fv; external opengl32;
procedure glRasterPos3i; external opengl32;
procedure glRasterPos3iv; external opengl32;
procedure glRasterPos3s; external opengl32;
procedure glRasterPos3sv; external opengl32;
procedure glRasterPos4d; external opengl32;
procedure glRasterPos4dv; external opengl32;
procedure glRasterPos4f; external opengl32;
procedure glRasterPos4fv; external opengl32;
procedure glRasterPos4i; external opengl32;
procedure glRasterPos4iv; external opengl32;
procedure glRasterPos4s; external opengl32;
procedure glRasterPos4sv; external opengl32;
procedure glReadBuffer; external opengl32;
procedure glReadPixels; external opengl32;
procedure glRectd; external opengl32;
procedure glRectdv; external opengl32;
procedure glRectf; external opengl32;
procedure glRectfv; external opengl32;
procedure glRecti; external opengl32;
procedure glRectiv; external opengl32;
procedure glRects; external opengl32;
procedure glRectsv; external opengl32;
function  glRenderMode; external opengl32;
procedure glRotated; external opengl32;
procedure glRotatef; external opengl32;
procedure glScaled; external opengl32;
procedure glScalef; external opengl32;
procedure glScissor; external opengl32;
procedure glSelectBuffer; external opengl32;
procedure glShadeModel; external opengl32;
procedure glStencilFunc; external opengl32;
procedure glStencilMask; external opengl32;
procedure glStencilOp; external opengl32;
procedure glTexCoord1d; external opengl32;
procedure glTexCoord1dv; external opengl32;
procedure glTexCoord1f; external opengl32;
procedure glTexCoord1fv; external opengl32;
procedure glTexCoord1i; external opengl32;
procedure glTexCoord1iv; external opengl32;
procedure glTexCoord1s; external opengl32;
procedure glTexCoord1sv; external opengl32;
procedure glTexCoord2d; external opengl32;
procedure glTexCoord2dv; external opengl32;
procedure glTexCoord2f; external opengl32;
procedure glTexCoord2fv; external opengl32;
procedure glTexCoord2i; external opengl32;
procedure glTexCoord2iv; external opengl32;
procedure glTexCoord2s; external opengl32;
procedure glTexCoord2sv; external opengl32;
procedure glTexCoord3d; external opengl32;
procedure glTexCoord3dv; external opengl32;
procedure glTexCoord3f; external opengl32;
procedure glTexCoord3fv; external opengl32;
procedure glTexCoord3i; external opengl32;
procedure glTexCoord3iv; external opengl32;
procedure glTexCoord3s; external opengl32;
procedure glTexCoord3sv; external opengl32;
procedure glTexCoord4d; external opengl32;
procedure glTexCoord4dv; external opengl32;
procedure glTexCoord4f; external opengl32;
procedure glTexCoord4fv; external opengl32;
procedure glTexCoord4i; external opengl32;
procedure glTexCoord4iv; external opengl32;
procedure glTexCoord4s; external opengl32;
procedure glTexCoord4sv; external opengl32;
procedure glTexEnvf; external opengl32;
procedure glTexEnvfv; external opengl32;
procedure glTexEnvi; external opengl32;
procedure glTexEnviv; external opengl32;
procedure glTexGend; external opengl32;
procedure glTexGendv; external opengl32;
procedure glTexGenf; external opengl32;
procedure glTexGenfv; external opengl32;
procedure glTexGeni; external opengl32;
procedure glTexGeniv; external opengl32;
procedure glTexImage1D; external opengl32;
procedure glTexImage2D; external opengl32;
procedure glTexParameterf; external opengl32;
procedure glTexParameterfv; external opengl32;
procedure glTexParameteri; external opengl32;
procedure glTexParameteriv; external opengl32;
procedure glTranslated; external opengl32;
procedure glTranslatef; external opengl32;
procedure glVertex2d; external opengl32;
procedure glVertex2dv; external opengl32;
procedure glVertex2f; external opengl32;
procedure glVertex2fv; external opengl32;
procedure glVertex2i; external opengl32;
procedure glVertex2iv; external opengl32;
procedure glVertex2s; external opengl32;
procedure glVertex2sv; external opengl32;
procedure glVertex3d; external opengl32;
procedure glVertex3dv; external opengl32;
procedure glVertex3f; external opengl32;
procedure glVertex3fv; external opengl32;
procedure glVertex3i; external opengl32;
procedure glVertex3iv; external opengl32;
procedure glVertex3s; external opengl32;
procedure glVertex3sv; external opengl32;
procedure glVertex4d; external opengl32;
procedure glVertex4dv; external opengl32;
procedure glVertex4f; external opengl32;
procedure glVertex4fv; external opengl32;
procedure glVertex4i; external opengl32;
procedure glVertex4iv; external opengl32;
procedure glVertex4s; external opengl32;
procedure glVertex4sv; external opengl32;
procedure glViewport; external opengl32;

function wglGetProcAddress;    external opengl32;

{ OpenGL Utility routines (glu.h) =======================================}

function gluErrorString;                     external glu32;
function gluErrorUnicodeStringEXT;           external glu32;
function gluGetString;                       external glu32;
procedure gluLookAt;                         external glu32;
procedure gluOrtho2D;                        external glu32;
procedure gluPerspective;                    external glu32;
procedure gluPickMatrix;                     external glu32;
function  gluProject;                        external glu32;
function  gluUnProject;                      external glu32;
function  gluScaleImage;                     external glu32;
function  gluBuild1DMipmaps;                 external glu32;
function  gluBuild2DMipmaps;                 external glu32;
function  gluNewQuadric;                     external glu32;
procedure gluDeleteQuadric;                  external glu32;
procedure gluQuadricNormals;                 external glu32;
procedure gluQuadricTexture;                 external glu32;
procedure gluQuadricOrientation;             external glu32;
procedure gluQuadricDrawStyle;               external glu32;
procedure gluCylinder;                       external glu32;
procedure gluDisk;                           external glu32;
procedure gluPartialDisk;                    external glu32;
procedure gluSphere;                         external glu32;
procedure gluQuadricCallback;                external glu32;

function gluNewTess                         ;external glu32;
procedure gluDeleteTess                     ;external glu32;
procedure gluTessBeginPolygon               ;external glu32;
procedure gluTessBeginContour               ;external glu32;
procedure gluTessVertex                     ;external glu32;
procedure gluTessEndContour                 ;external glu32;
procedure gluTessEndPolygon                 ;external glu32;
procedure gluTessProperty                   ;external glu32;
procedure gluTessNormal                     ;external glu32;
procedure gluTessCallback                   ;external glu32;

function gluNewNurbsRenderer                ;external glu32;
procedure gluDeleteNurbsRenderer            ;external glu32;
procedure gluBeginSurface                   ;external glu32;
procedure gluBeginCurve                     ;external glu32;
procedure gluEndCurve                       ;external glu32;
procedure gluEndSurface                     ;external glu32;
procedure gluBeginTrim                      ;external glu32;
procedure gluEndTrim                        ;external glu32;
procedure gluPwlCurve                       ;external glu32;
procedure gluNurbsCurve                     ;external glu32;
procedure gluNurbsSurface                   ;external glu32;
procedure gluLoadSamplingMatrices           ;external glu32;
procedure gluNurbsProperty                  ;external glu32;
procedure gluGetNurbsProperty               ;external glu32;
procedure gluNurbsCallback                  ;external glu32;


begin
  Set8087CW($133F);
end.
