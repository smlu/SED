#include "gl_view_common.hpp"

#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "render/lighting/dynamic_light_utils.hpp"
#include "render/mesh/wireframe_overlay_utils.hpp"
#include "render/overlays/frame_path_overlay_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"
#include "render/selection/vertex_marker_utils.hpp"
#include "render/translucency/translucent_order_utils.hpp"
#include "render/viewport_aa_policy.hpp"
#include "ui/viewport/toast_overlay_painter.hpp"
#include "sed/level/geometry.hpp"

#include <QByteArray>
#include <QFontMetricsF>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <QVariant>

namespace sed
{

namespace
{

using MultiDrawArraysFunction = void (QOPENGLF_APIENTRY *)(
    GLenum mode,
    const GLint* first,
    const GLsizei* count,
    GLsizei drawCount);

} // namespace


void GlView::initializeGL()
{
    QOpenGLContext* activeContext = QOpenGLContext::currentContext();
    if ( glResourcesContextLost_ )
    {
        if ( activeContext != context() )
        {
            return;
        }

        // Drop wrappers associated with the expired context while no context
        // is current. Their GL names died with that context and must not be
        // deleted from this newly-created context.
        doneCurrent();
        releaseOpenGlResources();
        makeCurrent();
        activeContext = QOpenGLContext::currentContext();
        if ( activeContext != context() )
        {
            return;
        }
    }

    if ( contextCleanupConnection_ )
    {
        QObject::disconnect(contextCleanupConnection_);
    }
    if ( activeContext )
    {
        contextCleanupConnection_ = QObject::connect(
            activeContext,
            &QOpenGLContext::aboutToBeDestroyed,
            this,
            [this]()
            {
                makeCurrent();
                if ( QOpenGLContext::currentContext() == context() )
                {
                    releaseOpenGlResources();
                    doneCurrent();
                }
                else
                {
                    deferOpenGlResourceCleanup();
                }
            },
            Qt::DirectConnection);
    }
    thingOverlayMultiDrawArraysFunction_ = activeContext
        ? activeContext->getProcAddress(QByteArrayLiteral("glMultiDrawArrays"))
        : nullptr;
    auto* f = activeContext ? activeContext->functions() : nullptr;
    if (!f)
    {
        return;
    }
    surfaceWireColorMaxTextureSize_ = 0;
    f->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &surfaceWireColorMaxTextureSize_);
    releaseDynamicLightUniformBuffer();
    dynamicLightUniformBufferWarningEmitted_ = false;
    f->glClearColor(
        backgroundClearColor_.redF(),
        backgroundClearColor_.greenF(),
        backgroundClearColor_.blueF(),
        1.0f);
    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_MULTISAMPLE);
    f->glEnable(GL_PROGRAM_POINT_SIZE);

    const char* vert = R"GLSL(
  #version 330 core
  #define SED_DYNAMIC_LIGHT_UBO 1
  layout(location = 0) in vec3 a_pos;
  layout(location = 1) in vec2 a_uv;
  layout(location = 2) in vec4 a_color;
  layout(location = 3) in float a_pick_id;
  layout(location = 4) in float a_geo;
  layout(location = 5) in float a_sky;
  layout(location = 6) in vec3 a_editor_sector_tint_scale;
  layout(location = 8) in vec3 a_normal;
  layout(location = 9) in float a_dynamic_light_target;
  layout(location = 10) in vec4 a_sector_light_pos_min;
  layout(location = 11) in vec4 a_sector_light_color_max;
  layout(location = 12) in vec3 a_face_light_anchor;
  layout(location = 13) in float a_sector_light_owner;
  layout(location = 14) in vec3 a_surface_wire_barycentric;
  layout(location = 15) in vec4 a_surface_wire_metadata;
  uniform mat4 u_mvp;
  uniform vec2 u_uv_scale;
  uniform vec2 u_uv_offset;
 uniform vec3 u_outline_center;
 uniform float u_outline_scale;
 uniform float u_outline_px;
  uniform vec2 u_viewport_size;
  uniform vec2 u_outline_dir;
  uniform float u_point_size;
  uniform float u_ndc_z_bias;
  uniform int u_render_pipeline;
  uniform vec3 u_camera_pos;
 uniform float u_sky_ceiling_height;
 uniform vec2 u_sky_ceiling_offset;
 uniform vec2 u_sky_canvas_half;
  uniform float u_sky_horizon_scale;
  uniform float u_sky_horizon_ndc_z;
  uniform vec2 u_sky_horizon_offset;
  uniform vec2 u_sky_horizon_yaw_pitch;
  uniform vec2 u_sky_horizon_roll_sin_cos;
  uniform int u_frame_xform_enabled;
  uniform int u_frame_xform_id;
  uniform vec3 u_frame_xform_color_mul;
  uniform mat4 u_frame_xform_matrix;
  uniform int u_xform_enabled;
  uniform int u_xform_count;
  uniform int u_xform_ids[256];
  uniform vec3 u_xform_color_mul[256];
  uniform mat4 u_xform_matrix;
  uniform int u_dynamic_light_project;
  uniform int u_dynamic_light_count;
  uniform bool u_dynamic_lights_per_pixel;
  uniform bool u_sector_point_lights_enabled;
  uniform int u_sector_light_preview_count;
  uniform int u_sector_light_preview_owners[16];
  uniform vec4 u_sector_light_preview_pos_min[16];
  uniform vec4 u_sector_light_preview_color_max[16];
  uniform bool u_apply_classic_sector_tint;
  uniform bool u_apply_local_classic_sector_tint;
  uniform vec3 u_classic_sector_tint_scale;
  uniform bool u_apply_classic_solid_light_ramp;
  uniform vec3 u_classic_solid_light_ramp[64];
  #if SED_DYNAMIC_LIGHT_UBO
  layout(std140) uniform DynamicLightBlock
  {
    vec4 u_dynamic_light_pos_min[64];
    vec4 u_dynamic_light_color_max[64];
  };
  #endif
  out vec3 v_uvq;
  out vec4 v_color;
  out vec3 v_world_pos;
  out vec3 v_world_normal;
   flat out vec3 v_face_normal;
   flat out int v_dynamic_light_target;
   flat out vec4 v_sector_light_pos_min;
   flat out vec4 v_sector_light_color_max;
   flat out vec3 v_dynamic_light_aux;
   flat out vec4 v_diffuse_back_color;
   flat out int v_use_diffuse_back_color;
   flat out int v_geo;
   flat out int v_sky;
   flat out vec3 v_classic_tint_scale;
   centroid noperspective out vec3 v_surface_wire_barycentric;
   flat out vec4 v_surface_wire_metadata;

   vec3 transformNormal(mat4 transformMatrix, vec3 normal)
   {
     if (dot(normal, normal) <= 1.0e-12)
     {
       return normal;
     }
     return normalize(transpose(inverse(mat3(transformMatrix))) * normal);
   }

   vec3 ijimModelLightContribution(
     vec3 lightPos,
     vec3 lightColor,
     float minRadius,
     float maxRadius,
     vec3 vertexPos,
     vec3 vertexNormal)
   {
     vec3 lightDir = lightPos - vertexPos;
     float dist = length(lightDir);
     if (!(dist > 1.0e-8) || !(dist < minRadius))
     {
       return vec3(0.0);
     }

     lightDir /= dist;
     float ndotl = dot(normalize(vertexNormal), lightDir);
     if (!(ndotl > 0.0))
     {
       return vec3(0.0);
     }

     float strength = 1.0;
     if (!(dist < maxRadius))
     {
       float span = minRadius - maxRadius;
       if (abs(span) <= 1.0e-8)
       {
         return vec3(0.0);
       }
       strength = 1.0 - ((dist - maxRadius) / span);
     }
     return lightColor * (ndotl * strength);
   }

   vec3 classicModelLightContribution(
     vec3 lightPos,
     float intensity,
     float minRadius,
     vec3 vertexPos,
     vec3 vertexNormal)
   {
     vec3 lightDir = lightPos - vertexPos;
     float dist = length(lightDir);
     if (!(dist > 1.0e-8) || !(dist < minRadius))
     {
       return vec3(0.0);
     }
     lightDir /= dist;
     float ndotl = dot(normalize(vertexNormal), lightDir);
     if (!(ndotl > 0.0))
     {
       return vec3(0.0);
     }
     return vec3((intensity - dist * 0.4) * ndotl);
   }

   vec3 ijimModelFaceLightContribution(
     vec3 lightPos,
     vec3 lightColor,
     float minRadius,
     vec3 faceAnchor,
     vec3 faceNormal,
     inout float attenuation)
   {
     vec3 normal = normalize(faceNormal);
     vec3 lightDir = lightPos - faceAnchor;
     float dist = length(lightDir);
     if (!(dist > 1.0e-8))
     {
       return vec3(0.0);
     }

     float planeDist = dot(lightDir, normal);
     if (!(planeDist < minRadius))
     {
       return vec3(0.0);
     }

     lightDir /= dist;
     float ndotl = dot(normal, lightDir);
     if (!(ndotl > 0.0))
     {
       return vec3(0.0);
     }
     // Preserve rdLight_CalcFaceIntensity's legacy state mutation: the input
     // attenuation is overwritten for every contributing light and reused by
     // the next light in camera collection order.
     attenuation = planeDist * attenuation;
     return (lightColor - vec3(attenuation)) * ndotl;
   }

   vec3 classicModelFaceLightContribution(
     vec3 lightPos,
     float intensity,
     float minRadius,
     vec3 faceAnchor,
     vec3 faceNormal)
   {
     vec3 normal = normalize(faceNormal);
     vec3 lightDir = lightPos - faceAnchor;
     float dist = length(lightDir);
     if (!(dist > 1.0e-8))
     {
       return vec3(0.0);
     }

     float planeDist = dot(lightDir, normal);
     if (!(planeDist < minRadius))
     {
       return vec3(0.0);
     }

     lightDir /= dist;
     float ndotl = dot(normal, lightDir);
     if (!(ndotl > 0.0))
     {
       return vec3(0.0);
     }
     // JK/MotS passes the camera's 0.4 attenuation into each independent
     // light calculation. Only IJIM mutates and reuses this state.
     return vec3((intensity - planeDist * 0.4) * ndotl);
   }

    void main() {
     vec3 worldPos = a_pos;
     vec3 worldNormal = a_normal;
     vec3 worldFaceAnchor = a_face_light_anchor;
     vec3 dynamicLightAux = a_face_light_anchor;
     vec4 outColor = a_color;
     int pid = int(a_pick_id + 0.5);
     if ((u_frame_xform_enabled != 0) && (pid == u_frame_xform_id))
     {
       vec4 t = u_frame_xform_matrix * vec4(worldPos, 1.0);
       worldPos = t.xyz;
       worldNormal = transformNormal(u_frame_xform_matrix, worldNormal);
       worldFaceAnchor = (u_frame_xform_matrix * vec4(worldFaceAnchor, 1.0)).xyz;
       outColor.rgb *= u_frame_xform_color_mul;
     }
     if (u_xform_enabled != 0)
     {
      for (int i = 0; i < u_xform_count; ++i)
      {
        int encodedId = u_xform_ids[i];
        bool sourceFiltered = encodedId < 0;
        int expectedId = sourceFiltered ? ~encodedId : encodedId;
        vec3 sourceDelta = worldPos - u_xform_color_mul[i];
        bool sourceMatches = !sourceFiltered || (dot(sourceDelta, sourceDelta) <= 1.0e-8);
        if ((pid == expectedId) && sourceMatches)
        {
          vec4 t = u_xform_matrix * vec4(worldPos, 1.0);
          worldPos = t.xyz;
          worldNormal = transformNormal(u_xform_matrix, worldNormal);
          worldFaceAnchor = (u_xform_matrix * vec4(worldFaceAnchor, 1.0)).xyz;
          if (!sourceFiltered)
          {
            outColor.rgb *= u_xform_color_mul[i];
          }
          break;
        }
      }
     }

     bool diffuseFace = a_dynamic_light_target > 2.5;
     vec4 backfaceColor = outColor;
     float diffuseFrontAttenuation = 0.4;
     float diffuseBackAttenuation = 0.4;
     float classicDiffuseFrontIntensity = 0.0;
     float classicDiffuseBackIntensity = 0.0;

     bool perPixelDynamic = false;
     #if SED_DYNAMIC_LIGHT_UBO
     // OpenJones3D's OpenGL path enables fragment lighting only for effective
     // Gouraud geometry. Diffuse keeps the original flat face-anchor result.
     perPixelDynamic = u_dynamic_lights_per_pixel
       && (a_dynamic_light_target > 0.5)
       && (a_dynamic_light_target < 2.5);
     #endif

     vec4 sectorLightPosMin = a_sector_light_pos_min;
     vec4 sectorLightColorMax = a_sector_light_color_max;
     int sectorLightOwner = a_sector_light_owner < -0.5
       ? -1
       : int(a_sector_light_owner + 0.5);
     if (a_dynamic_light_target > 1.5)
     {
       for (int i = 0; i < 16; ++i)
       {
         if (i >= u_sector_light_preview_count)
         {
           break;
         }
         if (sectorLightOwner == u_sector_light_preview_owners[i])
         {
           sectorLightPosMin = u_sector_light_preview_pos_min[i];
           sectorLightColorMax = u_sector_light_preview_color_max[i];
           break;
         }
       }
     }

     #if SED_DYNAMIC_LIGHT_UBO
     if (!perPixelDynamic && (a_dynamic_light_target > 0.5))
     {
       if (a_dynamic_light_target < 1.5)
       {
         // Both sithCamera implementations install 0.8 as the camera's
         // world-vertex attenuation before runtime light collection.
         float attenuationMax = 0.8;
         vec3 dynamicIntensity = vec3(0.0);
         for (int i = 0; i < 64; ++i)
         {
           if (i >= u_dynamic_light_count)
           {
             break;
           }
           vec3 delta = u_dynamic_light_pos_min[i].xyz - worldPos;
           float dist = length(delta);
           float radius = u_dynamic_light_color_max[i].w;
           if (dist < radius)
           {
             vec3 contribution = u_dynamic_light_color_max[i].rgb - vec3(dist * attenuationMax);
             // sithRender clamps the accumulated world-vertex intensity after
             // every emitter, including negative per-channel falloff terms.
             if (u_dynamic_light_project == 2)
             {
               dynamicIntensity = clamp(dynamicIntensity + contribution, vec3(0.0), vec3(1.0));
             }
             else
             {
               dynamicIntensity += contribution;
             }
           }
         }
         outColor.rgb += dynamicIntensity;
         if (u_dynamic_light_project == 2)
         {
           outColor.rgb = clamp(outColor.rgb, vec3(0.0), vec3(1.0));
         }
       }
       else
       {
         for (int i = 0; i < 64; ++i)
         {
           if (i >= u_dynamic_light_count)
           {
             break;
           }
           if (u_dynamic_light_project == 2)
           {
             if (diffuseFace)
             {
               outColor.rgb += ijimModelFaceLightContribution(
                   u_dynamic_light_pos_min[i].xyz,
                   u_dynamic_light_color_max[i].rgb,
                   u_dynamic_light_pos_min[i].w,
                   worldFaceAnchor,
                   worldNormal,
                   diffuseFrontAttenuation);
               backfaceColor.rgb += ijimModelFaceLightContribution(
                   u_dynamic_light_pos_min[i].xyz,
                   u_dynamic_light_color_max[i].rgb,
                   u_dynamic_light_pos_min[i].w,
                   worldFaceAnchor,
                   -worldNormal,
                   diffuseBackAttenuation);
             }
             else
             {
               outColor.rgb += ijimModelLightContribution(
                   u_dynamic_light_pos_min[i].xyz,
                   u_dynamic_light_color_max[i].rgb,
                   u_dynamic_light_pos_min[i].w,
                   u_dynamic_light_color_max[i].w,
                   worldPos,
                   worldNormal);
             }
           }
           else
           {
             if (diffuseFace)
             {
               // rdLight_CalcFaceIntensity stops after the first contribution
               // that reaches 1, retaining that contribution's overshoot.
               if (classicDiffuseFrontIntensity < 1.0)
               {
                 classicDiffuseFrontIntensity += classicModelFaceLightContribution(
                     u_dynamic_light_pos_min[i].xyz,
                     u_dynamic_light_color_max[i].r,
                     u_dynamic_light_pos_min[i].w,
                     worldFaceAnchor,
                     worldNormal).r;
               }
               if (classicDiffuseBackIntensity < 1.0)
               {
                 classicDiffuseBackIntensity += classicModelFaceLightContribution(
                     u_dynamic_light_pos_min[i].xyz,
                     u_dynamic_light_color_max[i].r,
                     u_dynamic_light_pos_min[i].w,
                     worldFaceAnchor,
                     -worldNormal).r;
               }
             }
             else
             {
               outColor.rgb += classicModelLightContribution(
                   u_dynamic_light_pos_min[i].xyz,
                   u_dynamic_light_color_max[i].r,
                   u_dynamic_light_pos_min[i].w,
                   worldPos,
                   worldNormal);
             }
             if ((u_dynamic_light_project == 1) && !diffuseFace)
             {
               outColor.rgb = min(outColor.rgb, vec3(1.0));
             }
           }
         }

         if ((u_dynamic_light_project != 2) && diffuseFace)
         {
           outColor.rgb += vec3(classicDiffuseFrontIntensity);
           backfaceColor.rgb += vec3(classicDiffuseBackIntensity);
         }

         // IJIM never rdLight_NewEntry-initializes its static sector-light
         // slot. Gouraud ignores bEnabled, but Diffuse checks its retained
         // zero value. Collected Thing lights above remain enabled in both.
         if ((u_dynamic_light_project == 2)
             && !diffuseFace
             && u_sector_point_lights_enabled)
         {
           outColor.rgb += ijimModelLightContribution(
             sectorLightPosMin.xyz,
             sectorLightColorMax.rgb,
             sectorLightPosMin.w,
             sectorLightColorMax.w,
             worldPos,
             worldNormal);
         }
       }
     }
     #endif

     if (!perPixelDynamic
         && (u_dynamic_light_project != 2)
         && (a_dynamic_light_target > 0.5))
     {
       // Attributes 10/11 carry classic signed face/surface extra light and
       // the ambient floor. Gouraud/world has rdPrimit3's authored+dynamic
       // clamp before rdCache adds extra; Diffuse has no such inner clamp.
       vec3 classicExtraLight = a_sector_light_pos_min.rgb;
       vec3 classicAmbientFloor = clamp(a_sector_light_color_max.rgb, vec3(0.0), vec3(1.0));
       if (a_dynamic_light_target < 2.5)
       {
         outColor.rgb = max(
           classicAmbientFloor,
           clamp(clamp(outColor.rgb, vec3(0.0), vec3(1.0)) + classicExtraLight, vec3(0.0), vec3(1.0)));
       }
       else
       {
         outColor.rgb = max(
           classicAmbientFloor,
           clamp(outColor.rgb + classicExtraLight, vec3(0.0), vec3(1.0)));
         backfaceColor.rgb = max(
           classicAmbientFloor,
           clamp(backfaceColor.rgb + classicExtraLight, vec3(0.0), vec3(1.0)));
       }
     }

     if (!perPixelDynamic
         && (u_dynamic_light_project == 2)
         && (a_dynamic_light_target > 0.5)
         && (a_dynamic_light_target < 1.5))
     {
       // IJIM level geometry is ordered differently from 3DO lighting:
       // rdClip clamps authored + dynamic first, then rdCache adds the signed
       // surface+sector extra light and performs the final clamp. Attribute 12
       // carries that combined extra value for world-light target vertices.
       outColor.rgb = clamp(
         clamp(outColor.rgb, vec3(0.0), vec3(1.0)) + dynamicLightAux,
         vec3(0.0),
         vec3(1.0));
     }

     if (!perPixelDynamic && (a_dynamic_light_target > 1.5))
     {
       // rdLight accumulates raw 3DO vertex intensity, face extra light and
       // camera ambient without normalizing. rdCache then clamps the final
       // vertex before interpolation and texture modulation. Deferring this
       // to the framebuffer makes an endpoint above 1.0 bleed excessive
       // brightness across the face and overpower its texture (most visible
       // with IJIM sector point lights). This remains required when the UBO
       // fallback shader disables runtime-light additions.
       outColor.rgb = clamp(outColor.rgb, vec3(0.0), vec3(1.0));
       if (diffuseFace)
       {
         backfaceColor.rgb = clamp(backfaceColor.rgb, vec3(0.0), vec3(1.0));
       }
     }

     bool motsRgbGouraud = (u_dynamic_light_project == 1)
       && (a_dynamic_light_target > 0.5)
       && (a_dynamic_light_target < 2.5);
     bool applyClassicSolidLightRamp = u_apply_classic_solid_light_ramp
       && (u_dynamic_light_project != 2)
       && !motsRgbGouraud
       && (int(a_geo + 0.5) == 3);
     if (!perPixelDynamic && applyClassicSolidLightRamp)
     {
       int frontLevel = clamp(int(clamp(outColor.r, 0.0, 1.0) * 63.0), 0, 63);
       outColor.rgb = u_classic_solid_light_ramp[frontLevel];
       if (diffuseFace)
       {
         int backLevel = clamp(int(clamp(backfaceColor.r, 0.0, 1.0) * 63.0), 0, 63);
         backfaceColor.rgb = u_classic_solid_light_ramp[backLevel];
       }
     }

     vec3 classicTintScale = u_apply_classic_sector_tint
       ? u_classic_sector_tint_scale
       : (u_apply_local_classic_sector_tint
           ? a_editor_sector_tint_scale
           : vec3(1.0));
     bool applyClassicTint = any(greaterThan(
       abs(classicTintScale - vec3(1.0)),
       vec3(1.0e-6)));
     if (!perPixelDynamic && applyClassicTint)
     {
       // Game Preview uses the camera-sector-wide engine effect. The editor
       // viewport uses local per-vertex Tint for authoring inspection.
       outColor.rgb = clamp(outColor.rgb * classicTintScale, vec3(0.0), vec3(1.0));
       if (diffuseFace)
       {
         backfaceColor.rgb = clamp(
           backfaceColor.rgb * classicTintScale,
           vec3(0.0),
           vec3(1.0));
       }
     }

    v_world_pos = worldPos;
    v_world_normal = worldNormal;
    v_face_normal = worldNormal;
    v_dynamic_light_target = int(a_dynamic_light_target + 0.5);
    v_sector_light_pos_min = sectorLightPosMin;
    v_sector_light_color_max = sectorLightColorMax;
    v_dynamic_light_aux = dynamicLightAux;
    v_classic_tint_scale = classicTintScale;
    vec3 pos = u_outline_center + (worldPos - u_outline_center) * u_outline_scale;
    vec4 clip = u_mvp * vec4(pos, 1.0);
    gl_Position = clip;

    if (u_outline_px > 0.0)
    {
      vec2 dir = u_outline_dir;
      float len = length(dir);
      if (len > 1e-6) { dir /= len; } else { dir = vec2(1.0, 0.0); }

      vec2 safeViewport = max(u_viewport_size, vec2(1.0, 1.0));
      vec2 pxToNdc = vec2(2.0 / safeViewport.x, 2.0 / safeViewport.y);
      vec2 offsetNdc = dir * u_outline_px * pxToNdc;
      gl_Position.xy += offsetNdc * gl_Position.w;
    }

    gl_PointSize = u_point_size;
    v_color = outColor;
    v_diffuse_back_color = backfaceColor;
    v_use_diffuse_back_color = diffuseFace ? 1 : 0;
    v_geo = int(a_geo + 0.5);
    v_sky = int(a_sky + 0.5);
    v_surface_wire_barycentric = a_surface_wire_barycentric;
    v_surface_wire_metadata = a_surface_wire_metadata;

    if ((u_render_pipeline != 0) && (v_sky == 1))
    {
      // Sky ceiling stays at the far plane so it never occludes world/thing geometry.
      gl_Position.z = gl_Position.w * 0.99996948;
    }
     else if ((u_render_pipeline != 0) && (v_sky == 2))
     {
       // Sky horizon: match engine behavior where horizon polys are drawn in screen space at a fixed depth
       // (prevents nearby geometry from being clipped/occluded by the horizon portal).
       gl_Position.z = gl_Position.w * u_sky_horizon_ndc_z;
     }

     if ((v_sky != 1) && (v_sky != 2) && (u_ndc_z_bias != 0.0))
     {
       gl_Position.z -= u_ndc_z_bias * gl_Position.w;
     }

     if ((u_render_pipeline != 0) && (v_sky == 3))
     {
       // Keep billboard sprites slightly inside the far clip plane. This inset
       // should stay tiny and independent of the generic sprite depth-bias:
       // tying it to u_ndc_z_bias makes large distant billboards clamp farther
       // inside the frustum than needed, which reads like premature clipping.
       float maxZ = gl_Position.w * 0.99996948;
       float inset = 1.0e-5 * abs(gl_Position.w);
       if (gl_Position.z > (maxZ - inset))
       {
         gl_Position.z = maxZ - inset;
       }
     }

     vec2 uv = a_uv * u_uv_scale + u_uv_offset;
     float q = 1.0;

    if ((u_render_pipeline != 0) && (v_sky != 0))
    {
      if (v_sky == 1)
      {
        vec3 dir = normalize(pos - u_camera_pos);
        float t = 1000.0;
        if (abs(dir.z) > 1e-6)
        {
          t = (u_sky_ceiling_height - u_camera_pos.z) / dir.z;
        }
        vec3 skyPos = u_camera_pos + dir * t;
        uv = (skyPos.xy + u_sky_ceiling_offset) * 16.0 * u_uv_scale;
        q = 1.0;
      }
      else if (v_sky == 2)
      {
       // Pascal ProjectPoint flips Y (screen coordinates grow down); match that here.
       vec2 ndc = vec2(clip.x, -clip.y) / clip.w;
        vec2 s = ndc * u_sky_canvas_half * u_sky_horizon_scale;
        float sinr = u_sky_horizon_roll_sin_cos.x;
        float cosr = u_sky_horizon_roll_sin_cos.y;
        float uu = (s.x * cosr - s.y * sinr) + u_sky_horizon_yaw_pitch.x + u_sky_horizon_offset.x;
        float vv = (s.y * cosr + s.x * sinr) + u_sky_horizon_yaw_pitch.y + u_sky_horizon_offset.y;
        uv = vec2(uu, vv);
        q = clip.w;
      }
    }

      v_uvq = vec3(uv * q, q);
    }
    )GLSL";

    const char* frag = R"GLSL(
 #version 330 core
 #define SED_DYNAMIC_LIGHT_UBO 1
 uniform vec4 u_color;
 uniform bool u_textured;
 uniform sampler2D u_tex;
 uniform bool u_alpha_test;
 uniform bool u_alpha_test_final;
 uniform bool u_force_opaque_alpha;
 uniform float u_alpha_ref;
 uniform bool u_surface_wire_overlay_enabled;
 uniform bool u_surface_wire_antialiasing_enabled;
 uniform bool u_surface_wire_layer_colors_enabled;
 uniform bool u_grid_minor_point_mode;
 uniform float u_grid_minor_point_diameter_px;
 uniform float u_grid_minor_point_raster_size_px;
 uniform float u_surface_wire_width_px;
 uniform vec4 u_surface_wire_color;
 uniform sampler2D u_surface_wire_color_texture;
 uniform int u_surface_wire_color_count;
 /* SED_SURFACE_WIRE_COVERAGE_FUNCTION */
 uniform int u_lighting_mode;
 uniform int u_geo_min;
 uniform int u_sprite_preview_geometry;
 uniform bool u_flat_shade;
 uniform bool u_fog_enabled;
 uniform vec4 u_fog_color;
 uniform float u_fog_start;
 uniform float u_fog_end;
 uniform vec3 u_camera_pos;
 uniform int u_dynamic_light_project;
 uniform int u_dynamic_light_count;
 uniform bool u_dynamic_lights_per_pixel;
 uniform bool u_sector_point_lights_enabled;
 uniform bool u_apply_classic_sector_tint;
 uniform vec3 u_classic_sector_tint_scale;
 uniform bool u_apply_classic_solid_light_ramp;
 uniform vec3 u_classic_solid_light_ramp[64];
 #if SED_DYNAMIC_LIGHT_UBO
 layout(std140) uniform DynamicLightBlock
 {
   vec4 u_dynamic_light_pos_min[64];
   vec4 u_dynamic_light_color_max[64];
 };
 #endif
 in vec3 v_uvq;
 in vec4 v_color;
 in vec3 v_world_pos;
 in vec3 v_world_normal;
 flat in vec3 v_face_normal;
 flat in int v_dynamic_light_target;
 flat in vec4 v_sector_light_pos_min;
 flat in vec4 v_sector_light_color_max;
 flat in vec3 v_dynamic_light_aux;
 flat in vec4 v_diffuse_back_color;
 flat in int v_use_diffuse_back_color;
 flat in int v_geo;
 flat in vec3 v_classic_tint_scale;
 centroid noperspective in vec3 v_surface_wire_barycentric;
 flat in vec4 v_surface_wire_metadata;
 out vec4 fragColor;

 vec3 ijimModelLightContribution(
   vec3 lightPos,
   vec3 lightColor,
   float minRadius,
   float maxRadius,
   vec3 samplePos,
   vec3 sampleNormal)
 {
   vec3 lightDir = lightPos - samplePos;
   float dist = length(lightDir);
   if (!(dist > 1.0e-8))
   {
     return vec3(0.0);
   }
   lightDir /= dist;
   float ndotl = max(dot(normalize(sampleNormal), lightDir), 0.0);
   float innerRadius = maxRadius * 0.75;
   float outerRadius = minRadius * 1.25;
   float attenuation = 1.0 - smoothstep(innerRadius, outerRadius, dist);
   return lightColor * (ndotl * attenuation);
 }

 vec3 classicModelLightContribution(
   vec3 lightPos,
   float intensity,
   float minRadius,
   vec3 samplePos,
   vec3 sampleNormal)
 {
   vec3 lightDir = lightPos - samplePos;
   float dist = length(lightDir);
   if (!(dist > 1.0e-8) || !(dist < minRadius))
   {
     return vec3(0.0);
   }
   lightDir /= dist;
   float ndotl = dot(normalize(sampleNormal), lightDir);
   if (!(ndotl > 0.0))
   {
     return vec3(0.0);
   }
   return vec3((intensity - dist * 0.4) * ndotl);
 }

 void main() {
   // Only the isolated SPR authoring view supplies modes 1/2.
   // Normal scene batches retain -1. Normalized quad UVs describe the four
   // card edges independently of its two triangles, avoiding a wire diagonal.
   if ((u_sprite_preview_geometry == 1) || (u_sprite_preview_geometry == 2)) {
     vec2 uv = v_uvq.xy;
     if (abs(v_uvq.z) > 1e-6) {
       uv /= v_uvq.z;
     }
     vec2 edgePixels = min(uv, vec2(1.0) - uv)
       / max(fwidth(uv), vec2(1.0e-7));
     bool covered = (u_sprite_preview_geometry == 1)
       ? all(lessThan(edgePixels, vec2(2.0)))
       : any(lessThan(edgePixels, vec2(1.25)));
     if (!covered) {
       discard;
     }
     fragColor = vec4(1.0);
     return;
   }
   if (v_geo < u_geo_min) {
     discard;
   }

   vec4 frontColor = v_color;
   vec4 backColor = v_diffuse_back_color;
   bool motsRgbGouraud = (u_dynamic_light_project == 1)
     && (v_dynamic_light_target > 0)
     && (v_dynamic_light_target < 3);
   bool classicSolidLightRamp = u_apply_classic_solid_light_ramp
     && (u_dynamic_light_project != 2)
     && !motsRgbGouraud
     && (v_geo == 3);
   #if SED_DYNAMIC_LIGHT_UBO
   if (u_dynamic_lights_per_pixel
       && (v_dynamic_light_target > 0)
       && (v_dynamic_light_target < 3))
   {
     if (v_dynamic_light_target == 1)
     {
       vec3 dynamicIntensity = vec3(0.0);
       for (int i = 0; i < 64; ++i)
       {
         if (i >= u_dynamic_light_count)
         {
           break;
         }
         float dist = length(u_dynamic_light_pos_min[i].xyz - v_world_pos);
         if (u_dynamic_light_project == 2)
         {
           float radius = u_dynamic_light_color_max[i].w;
           float innerRange = radius * 0.75;
           float outerRange = radius * 1.25;
           float range = 1.0 - smoothstep(innerRange, outerRange, dist);
           vec3 contribution =
             (u_dynamic_light_color_max[i].rgb - vec3(dist * 0.8)) * range;
           dynamicIntensity += max(contribution, vec3(0.0));
         }
         else if (dist < u_dynamic_light_color_max[i].w)
         {
           dynamicIntensity += u_dynamic_light_color_max[i].rgb - vec3(dist * 0.8);
         }
       }
       if (u_dynamic_light_project == 2)
       {
         dynamicIntensity = clamp(dynamicIntensity, vec3(0.0), vec3(1.0));
       }
       frontColor.rgb += dynamicIntensity;
     }
     else
     {
       vec3 frontNormal = normalize(v_world_normal);

       for (int i = 0; i < 64; ++i)
       {
         if (i >= u_dynamic_light_count)
         {
           break;
         }
         if (u_dynamic_light_project == 2)
         {
           frontColor.rgb += ijimModelLightContribution(
             u_dynamic_light_pos_min[i].xyz,
             u_dynamic_light_color_max[i].rgb,
             u_dynamic_light_pos_min[i].w,
             u_dynamic_light_color_max[i].w,
             v_world_pos,
             frontNormal);
         }
         else
         {
           frontColor.rgb += classicModelLightContribution(
             u_dynamic_light_pos_min[i].xyz,
             u_dynamic_light_color_max[i].r,
             u_dynamic_light_pos_min[i].w,
             v_world_pos,
             frontNormal);
           if (u_dynamic_light_project == 1)
           {
             frontColor.rgb = min(frontColor.rgb, vec3(1.0));
           }
         }
       }

       if (u_dynamic_light_project == 2
           && u_sector_point_lights_enabled)
       {
         frontColor.rgb += ijimModelLightContribution(
           v_sector_light_pos_min.xyz,
           v_sector_light_color_max.rgb,
           v_sector_light_pos_min.w,
           v_sector_light_color_max.w,
           v_world_pos,
           frontNormal);
       }
     }

     if (u_dynamic_light_project != 2)
     {
       vec3 extraLight = v_sector_light_pos_min.rgb;
       vec3 ambientFloor = clamp(v_sector_light_color_max.rgb, vec3(0.0), vec3(1.0));
       if (v_dynamic_light_target < 3)
       {
         frontColor.rgb = max(
           ambientFloor,
           clamp(clamp(frontColor.rgb, vec3(0.0), vec3(1.0)) + extraLight, vec3(0.0), vec3(1.0)));
       }
       else
       {
         frontColor.rgb = max(ambientFloor, clamp(frontColor.rgb + extraLight, vec3(0.0), vec3(1.0)));
         backColor.rgb = max(ambientFloor, clamp(backColor.rgb + extraLight, vec3(0.0), vec3(1.0)));
       }
     }
     else if (v_dynamic_light_target == 1)
     {
       frontColor.rgb = clamp(
         clamp(frontColor.rgb, vec3(0.0), vec3(1.0)) + v_dynamic_light_aux,
         vec3(0.0),
         vec3(1.0));
     }

     if (v_dynamic_light_target > 1)
     {
       frontColor.rgb = clamp(frontColor.rgb, vec3(0.0), vec3(1.0));
       if (v_dynamic_light_target == 3)
       {
         backColor.rgb = clamp(backColor.rgb, vec3(0.0), vec3(1.0));
       }
     }
     bool applyClassicTint = any(greaterThan(
       abs(v_classic_tint_scale - vec3(1.0)),
       vec3(1.0e-6)));
     if (applyClassicTint && !classicSolidLightRamp)
     {
       frontColor.rgb = clamp(frontColor.rgb * v_classic_tint_scale, vec3(0.0), vec3(1.0));
       if (v_dynamic_light_target == 3)
       {
         backColor.rgb = clamp(backColor.rgb * v_classic_tint_scale, vec3(0.0), vec3(1.0));
       }
     }
   }
   #endif

   vec4 faceColor = ((v_use_diffuse_back_color != 0) && !gl_FrontFacing)
     ? backColor
     : frontColor;
   vec4 lit = (u_lighting_mode == 0) ? vec4(1.0, 1.0, 1.0, faceColor.a) : faceColor;
   if (classicSolidLightRamp)
   {
     if (u_lighting_mode == 0)
     {
       lit.rgb = u_classic_solid_light_ramp[63];
     }
     else if (u_dynamic_lights_per_pixel
         && (v_dynamic_light_target > 0)
         && (v_dynamic_light_target < 3))
     {
       int lightLevel = clamp(int(clamp(lit.r, 0.0, 1.0) * 63.0), 0, 63);
       lit.rgb = u_classic_solid_light_ramp[lightLevel];
     }
     if ((u_lighting_mode != 0)
         && any(greaterThan(
           abs(v_classic_tint_scale - vec3(1.0)),
           vec3(1.0e-6))))
     {
       lit.rgb = clamp(
         lit.rgb * v_classic_tint_scale,
         vec3(0.0),
         vec3(1.0));
     }
   }
   if (u_force_opaque_alpha) {
     // Editor overlays which reuse scene vertex ranges must not inherit the
     // source material's authored/translucent vertex alpha. This preserves
     // the solid semantics of the former copied-outline buffer.
     lit.a = 1.0;
   }
   if (any(greaterThan(
         abs(v_classic_tint_scale - vec3(1.0)),
         vec3(1.0e-6)))
       && (u_lighting_mode == 0)) {
     // Fully Lit is selected in this stage rather than the vertex stage, so
     // apply the same constant per-vertex effect before material modulation.
     lit.rgb = clamp(lit.rgb * v_classic_tint_scale, vec3(0.0), vec3(1.0));
   }
   bool doTexture = u_textured && (v_geo >= 4);
   if (doTexture) {
     vec2 uv = v_uvq.xy;
     if (abs(v_uvq.z) > 1e-6) {
       uv /= v_uvq.z;
     }
     vec4 tex = texture(u_tex, uv);
     vec4 shaded = tex * lit;
     float testedAlpha = u_alpha_test_final ? shaded.a : tex.a;
     if (u_alpha_test && (testedAlpha < u_alpha_ref)) {
       discard;
     }
     fragColor = shaded;
   } else {
    fragColor = classicSolidLightRamp
      ? lit
      : (u_color * lit);
    if (u_alpha_test && u_alpha_test_final && (fragColor.a < u_alpha_ref)) {
      discard;
    }
  }
  if (u_flat_shade) {
    vec3 N = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
    vec3 L = normalize(vec3(0.2, 0.3, 0.9));
    float diffuse = max(dot(N, L), 0.0);
    float shade = 0.35 + 0.65 * diffuse;
    fragColor.rgb *= shade;
  }
  if (u_fog_enabled) {
    float span = max(u_fog_end - u_fog_start, 1.0e-6);
    // The legacy renderers scale only the table fog start/end values. Scaling
    // the distance here as well cancels that visual tuning and makes IJIM fog
    // effectively invisible across ordinary editor view distances.
    float fogCoord = length(v_world_pos - u_camera_pos);
    float fogAmount = clamp((fogCoord - u_fog_start) / span, 0.0, 1.0);
    fragColor.rgb = mix(fragColor.rgb, u_fog_color.rgb, fogAmount);
  }
  if (u_surface_wire_overlay_enabled
      && any(greaterThan(v_surface_wire_metadata.xyz, vec3(0.5))))
  {
    // These vary in screen space and survive only where the face's regular
    // alpha/discard and depth tests already accepted a filled fragment. The
    // centroid qualifier keeps edge interpolation inside partially covered
    // MSAA pixels. The Euclidean gradient length measures the perpendicular
    // distance to each projected edge in physical pixels at every angle.
    vec3 barycentricGradientPx = vec3(
      length(vec2(dFdx(v_surface_wire_barycentric.x), dFdy(v_surface_wire_barycentric.x))),
      length(vec2(dFdx(v_surface_wire_barycentric.y), dFdy(v_surface_wire_barycentric.y))),
      length(vec2(dFdx(v_surface_wire_barycentric.z), dFdy(v_surface_wire_barycentric.z))));
    vec3 edgeDistancePx = abs(v_surface_wire_barycentric)
      / max(barycentricGradientPx, vec3(1.0e-6));
    float inwardWidthPx = max(u_surface_wire_width_px, 1.0);
    float wireCoverage = 0.0;
    if (v_surface_wire_metadata.x > 0.5)
    {
      wireCoverage = max(
        wireCoverage,
        surfaceWireOverlayCoverage(edgeDistancePx.x, inwardWidthPx));
    }
    if (v_surface_wire_metadata.y > 0.5)
    {
      wireCoverage = max(
        wireCoverage,
        surfaceWireOverlayCoverage(edgeDistancePx.y, inwardWidthPx));
    }
    if (v_surface_wire_metadata.z > 0.5)
    {
      wireCoverage = max(
        wireCoverage,
        surfaceWireOverlayCoverage(edgeDistancePx.z, inwardWidthPx));
    }

    if (wireCoverage > 0.0)
    {
      vec3 wireColor = u_surface_wire_color.rgb;
      int layerIndex = int(v_surface_wire_metadata.w);
      if (u_surface_wire_layer_colors_enabled
          && u_surface_wire_color_count > 0
          && layerIndex >= 0
          && layerIndex < u_surface_wire_color_count)
      {
        vec4 layerColor = texture(
          u_surface_wire_color_texture,
          vec2((float(layerIndex) + 0.5) / float(u_surface_wire_color_count), 0.5));
        if (layerColor.a > 0.5)
        {
          wireColor = layerColor.rgb;
        }
      }
      fragColor.rgb = mix(fragColor.rgb, wireColor, wireCoverage);
    }
  }
  if (u_grid_minor_point_mode)
  {
    float diameterPx = max(u_grid_minor_point_diameter_px, 0.0);
    float rasterSizePx = max(u_grid_minor_point_raster_size_px, 1.0);
    float radialDistancePx = length((gl_PointCoord - vec2(0.5)) * rasterSizePx);
    float pointCoverage = clamp(0.5 * diameterPx + 0.5 - radialDistancePx, 0.0, 1.0);
    fragColor.a *= pointCoverage;
    if (fragColor.a <= 1.0e-4)
    {
      discard;
    }
  }
 }
 )GLSL";

    const auto linkMainProgram = [&](const bool useDynamicLightBlock, QString& log) -> bool
    {
        auto candidate = std::make_unique<QOpenGLShaderProgram>();
        QByteArray vertexSource(vert);
        QByteArray fragmentSource = render::injectSurfaceWireCoverageFunction(
            QByteArray(frag));
        if (!useDynamicLightBlock)
        {
            vertexSource.replace(
                "#define SED_DYNAMIC_LIGHT_UBO 1",
                "#define SED_DYNAMIC_LIGHT_UBO 0");
            fragmentSource.replace(
                "#define SED_DYNAMIC_LIGHT_UBO 1",
                "#define SED_DYNAMIC_LIGHT_UBO 0");
        }
        const bool linked = candidate->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource)
            && candidate->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource)
            && candidate->link();
        log = candidate->log();
        if (linked)
        {
            program_ = std::move(candidate);
        }
        return linked;
    };

    bool dynamicLightShaderBlockLinked = false;
    QString mainShaderLog{};
    if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
    {
        dynamicLightShaderBlockLinked = true;
        if (!linkMainProgram(true, mainShaderLog))
        {
            dynamicLightShaderBlockLinked = false;
            dynamicLightUniformBufferWarningEmitted_ = true;
            Q_EMIT warningMessage(QString(
                "Dynamic-light uniform-block shader unavailable; dynamic runtime lights are disabled: %1")
                                      .arg(mainShaderLog));
            if (!linkMainProgram(false, mainShaderLog))
            {
                Q_EMIT errorMessage(QString("Failed to compile/link main shader fallback: %1").arg(mainShaderLog));
                program_.reset();
                return;
            }
        }
    }
    else if (!linkMainProgram(false, mainShaderLog))
    {
        Q_EMIT errorMessage(QString("Failed to compile/link main shader: %1").arg(mainShaderLog));
        program_.reset();
        return;
    }

    const auto warnDynamicLightBufferOnce = [this](const QString& detail)
    {
        dynamicLightUniformBufferReady_ = false;
        if (!dynamicLightUniformBufferWarningEmitted_)
        {
            dynamicLightUniformBufferWarningEmitted_ = true;
            Q_EMIT warningMessage(QString(
                "Dynamic runtime lights are disabled: %1").arg(detail));
        }
    };

    if (dynamicLightShaderBlockLinked)
    {
        QOpenGLContext* context = QOpenGLContext::currentContext();
        QOpenGLExtraFunctions* extra = context ? context->extraFunctions() : nullptr;
        if (!extra)
        {
            warnDynamicLightBufferOnce(QStringLiteral("OpenGL 3.3 uniform-buffer functions are unavailable."));
        }
        else
        {
            extra->initializeOpenGLFunctions();
            const GLuint blockIndex = extra->glGetUniformBlockIndex(
                program_->programId(),
                "DynamicLightBlock");
            if (blockIndex == GL_INVALID_INDEX)
            {
                warnDynamicLightBufferOnce(QStringLiteral("the linked shader has no DynamicLightBlock."));
            }
            else
            {
                GLint blockBytes = 0;
                GLint maxBlockBytes = 0;
                extra->glGetActiveUniformBlockiv(
                    program_->programId(),
                    blockIndex,
                    GL_UNIFORM_BLOCK_DATA_SIZE,
                    &blockBytes);
                extra->glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxBlockBytes);
                if (blockBytes != static_cast<GLint>(render::lighting::kDynamicLightGpuBlockBytes)
                    || maxBlockBytes < blockBytes)
                {
                    warnDynamicLightBufferOnce(QString(
                        "DynamicLightBlock layout/size is unsupported (shader %1 bytes, limit %2 bytes).")
                                                   .arg(blockBytes)
                                                   .arg(maxBlockBytes));
                }
                else
                {
                    GLint previousUniformBuffer = 0;
                    extra->glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousUniformBuffer);
                    extra->glGenBuffers(1, &dynamicLightUniformBuffer_);
                    if (dynamicLightUniformBuffer_ != 0)
                    {
                        const render::lighting::DynamicLightGpuBlock emptyBlock{};
                        extra->glBindBuffer(GL_UNIFORM_BUFFER, dynamicLightUniformBuffer_);
                        extra->glBufferData(
                            GL_UNIFORM_BUFFER,
                            static_cast<GLsizeiptr>(sizeof(emptyBlock)),
                            &emptyBlock,
                            GL_DYNAMIC_DRAW);
                        GLint allocatedBytes = 0;
                        extra->glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &allocatedBytes);
                        extra->glBindBuffer(
                            GL_UNIFORM_BUFFER,
                            static_cast<GLuint>(previousUniformBuffer));

                        if (allocatedBytes == static_cast<GLint>(sizeof(emptyBlock)))
                        {
                            extra->glUniformBlockBinding(
                                program_->programId(),
                                blockIndex,
                                render::lighting::kDynamicLightUniformBlockBinding);
                            GLint linkedBinding = -1;
                            extra->glGetActiveUniformBlockiv(
                                program_->programId(),
                                blockIndex,
                                GL_UNIFORM_BLOCK_BINDING,
                                &linkedBinding);
                            dynamicLightUniformBufferReady_ = linkedBinding
                                == static_cast<GLint>(render::lighting::kDynamicLightUniformBlockBinding);
                        }
                    }
                    else
                    {
                        extra->glBindBuffer(
                            GL_UNIFORM_BUFFER,
                            static_cast<GLuint>(previousUniformBuffer));
                    }

                    if (!dynamicLightUniformBufferReady_)
                    {
                        releaseDynamicLightUniformBuffer();
                        warnDynamicLightBufferOnce(QStringLiteral(
                            "uniform-buffer allocation or block binding failed."));
                    }
                }
            }
        }
    }

    program_->bind();
    program_->setUniformValue("u_tex", 0);
    program_->setUniformValue("u_point_size", 1.0f);
    program_->setUniformValue("u_grid_minor_point_mode", false);
    program_->setUniformValue("u_grid_minor_point_diameter_px", 2.0f);
    program_->setUniformValue("u_grid_minor_point_raster_size_px", 3.0f);
    program_->setUniformValue("u_ndc_z_bias", 0.0f);
    program_->setUniformValue("u_lighting_mode", 0);
    program_->setUniformValue("u_geo_min", 0);
    program_->setUniformValue("u_sprite_preview_geometry", -1);
    program_->setUniformValue("u_alpha_test", 0);
    program_->setUniformValue("u_alpha_test_final", 0);
    program_->setUniformValue("u_force_opaque_alpha", false);
    program_->setUniformValue("u_alpha_ref", 0.01f);
    program_->setUniformValue("u_outline_center", QVector3D(0.0f, 0.0f, 0.0f));
    program_->setUniformValue("u_outline_scale", 1.0f);
    program_->setUniformValue("u_outline_px", 0.0f);
    program_->setUniformValue("u_viewport_size", QVector2D(1.0f, 1.0f));
    program_->setUniformValue("u_outline_dir", QVector2D(1.0f, 0.0f));
    program_->setUniformValue("u_flat_shade", false);
    program_->setUniformValue("u_render_pipeline", 0);
    program_->setUniformValue("u_camera_pos", QVector3D(0.0f, 0.0f, 0.0f));
    program_->setUniformValue("u_sky_ceiling_height", 0.0f);
    program_->setUniformValue("u_sky_ceiling_offset", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_sky_canvas_half", QVector2D(1.0f, 1.0f));
    program_->setUniformValue("u_sky_horizon_scale", 0.0f);
    program_->setUniformValue("u_sky_horizon_ndc_z", 1.0f);
    program_->setUniformValue("u_sky_horizon_offset", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_sky_horizon_yaw_pitch", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_sky_horizon_roll_sin_cos", QVector2D(0.0f, 1.0f));
    program_->setUniformValue("u_fog_enabled", false);
    program_->setUniformValue("u_fog_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_fog_start", 0.0f);
    program_->setUniformValue("u_fog_end", 1.0f);
    program_->setUniformValue("u_frame_xform_enabled", 0);
    program_->setUniformValue("u_frame_xform_id", 0);
    program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
    program_->setUniformValue("u_xform_enabled", 0);
    program_->setUniformValue("u_xform_count", 0);
    program_->setUniformValue("u_xform_matrix", QMatrix4x4());
    program_->setUniformValue("u_dynamic_light_project", 0);
    program_->setUniformValue("u_dynamic_light_count", 0);
    program_->setUniformValue("u_dynamic_lights_per_pixel", false);
    program_->setUniformValue("u_sector_point_lights_enabled", false);
    program_->setUniformValue("u_apply_classic_sector_tint", false);
    program_->setUniformValue("u_apply_local_classic_sector_tint", false);
    program_->setUniformValue("u_apply_classic_solid_light_ramp", false);
    program_->setUniformValue(
        "u_classic_sector_tint_scale",
        QVector3D(1.0f, 1.0f, 1.0f));
    program_->release();

    thickLineProgram_ = std::make_unique<QOpenGLShaderProgram>();
    const char* thickLineVert = R"GLSL(
#version 330 core
 layout(location = 0) in vec3 a_pos;
 layout(location = 3) in float a_pick_id;
 layout(location = 6) in vec3 a_plane_ref;
 layout(location = 7) in float a_plane_valid;
 uniform mat4 u_mvp;
uniform int u_frame_xform_enabled;
uniform int u_frame_xform_id;
uniform mat4 u_frame_xform_matrix;
uniform int u_xform_enabled;
uniform int u_xform_count;
uniform int u_xform_ids[256];
uniform vec3 u_xform_color_mul[256];
uniform mat4 u_xform_matrix;
out vec4 v_plane_ref_clip;
out float v_plane_valid;
 void main()
 {
     vec3 worldPos = a_pos;
     vec3 planeRef = a_plane_ref;
     float planeValid = a_plane_valid;
     int pid = int(a_pick_id + 0.5);
     if ((u_frame_xform_enabled != 0) && (pid == u_frame_xform_id))
     {
         vec4 t = u_frame_xform_matrix * vec4(worldPos, 1.0);
         worldPos = t.xyz;
         if (planeValid > 0.5)
         {
             planeRef = (u_frame_xform_matrix * vec4(planeRef, 1.0)).xyz;
         }
     }
     if (u_xform_enabled != 0)
     {
         for (int i = 0; i < u_xform_count; ++i)
         {
             int encodedId = u_xform_ids[i];
             bool sourceFiltered = encodedId < 0;
             int expectedId = sourceFiltered ? ~encodedId : encodedId;
             if (pid != expectedId)
             {
                 continue;
             }

             if (sourceFiltered)
             {
                 // Shared-corner previews carry the exact original point in
                 // the payload. Transform the endpoint and supporting-plane
                 // reference independently so coplanar depth stays exact.
                 vec3 sourcePoint = u_xform_color_mul[i];
                 vec3 sourceDelta = worldPos - sourcePoint;
                 if (dot(sourceDelta, sourceDelta) <= 1.0e-8)
                 {
                     worldPos = (u_xform_matrix * vec4(worldPos, 1.0)).xyz;
                 }
                 if (planeValid > 0.5)
                 {
                     vec3 planeDelta = planeRef - sourcePoint;
                     if (dot(planeDelta, planeDelta) <= 1.0e-8)
                     {
                         planeRef = (u_xform_matrix * vec4(planeRef, 1.0)).xyz;
                     }
                 }
                 // One line vertex and its supporting-plane reference can
                 // correspond to different moved corners of the same face.
                 // Keep scanning filtered entries so both are transformed.
                 continue;
             }
             else
             {
                 worldPos = (u_xform_matrix * vec4(worldPos, 1.0)).xyz;
                 if (planeValid > 0.5)
                 {
                     planeRef = (u_xform_matrix * vec4(planeRef, 1.0)).xyz;
                 }
                 break;
             }
         }
     }

    gl_Position = u_mvp * vec4(worldPos, 1.0);
    v_plane_ref_clip = u_mvp * vec4(planeRef, 1.0);
    v_plane_valid = planeValid;
}
)GLSL";

    const char* thickLineGeom = R"GLSL(
#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 8) out;

in vec4 v_plane_ref_clip[];
in float v_plane_valid[];

uniform vec2 u_viewport_size;
uniform float u_line_width_px;
uniform float u_ndc_z_bias;
uniform int u_surface_depth_match;
uniform float u_plane_min_sin_squared;
uniform float u_clip_min_w;

float homogeneousClipDistance(vec4 point, int plane)
{
    if (plane == 0) return point.x + point.w;
    if (plane == 1) return -point.x + point.w;
    if (plane == 2) return point.y + point.w;
    if (plane == 3) return -point.y + point.w;
    if (plane == 4) return point.z + point.w;
    if (plane == 5) return -point.z + point.w;
    return point.w - u_clip_min_w;
}

bool clipLineToHomogeneousFrustum(inout vec4 p0c, inout vec4 p1c)
{
    // Calculate the accepted interval against every plane before changing the
    // endpoints. A rejection therefore cannot publish a half-clipped segment.
    vec4 source0 = p0c;
    vec4 source1 = p1c;
    float firstT = 0.0;
    float secondT = 1.0;
    for (int plane = 0; plane < 7; ++plane)
    {
        float d0 = homogeneousClipDistance(source0, plane);
        float d1 = homogeneousClipDistance(source1, plane);
        bool outside0 = d0 < 0.0;
        bool outside1 = d1 < 0.0;
        if (outside0 && outside1)
        {
            return false;
        }
        if (outside0 == outside1)
        {
            continue;
        }

        float denominator = d0 - d1;
        if (abs(denominator) <= 1.0e-20)
        {
            return false;
        }
        float t = clamp(d0 / denominator, 0.0, 1.0);
        if (outside0)
        {
            firstT = max(firstT, t);
        }
        else
        {
            secondT = min(secondT, t);
        }
        if (firstT > secondT)
        {
            return false;
        }
    }

    p0c = mix(source0, source1, firstT);
    p1c = mix(source0, source1, secondT);
    return (p0c.w >= u_clip_min_w) && (p1c.w >= u_clip_min_w);
}

bool supportingSurfaceDepthGradient(
    vec4 p0c,
    vec4 p1c,
    vec4 refc,
    out vec2 gradient,
    out vec2 projectedEdge,
    out float referenceSideCross)
{
    gradient = vec2(0.0);
    projectedEdge = vec2(0.0);
    referenceSideCross = 0.0;
    if ((p0c.w <= u_clip_min_w)
        || (p1c.w <= u_clip_min_w)
        || (refc.w <= u_clip_min_w))
    {
        return false;
    }

    // This point only reconstructs the supporting triangle's screen-depth
    // plane; it is not rasterized. A finite positive-W reference outside the
    // near clip plane still lies on that plane and must remain usable after
    // the visible edge has been clipped.
    vec3 p0 = p0c.xyz / p0c.w;
    vec3 p1 = p1c.xyz / p1c.w;
    vec3 refp = refc.xyz / refc.w;
    vec2 edge = p1.xy - p0.xy;
    vec2 toRef = refp.xy - p0.xy;
    float det = edge.x * toRef.y - edge.y * toRef.x;
    float determinantScale = dot(edge, edge) * dot(toRef, toRef);
    if ((determinantScale <= 1e-20)
        || ((det * det) <= determinantScale * max(u_plane_min_sin_squared, 0.0)))
    {
        return false;
    }

    float edgeZ = p1.z - p0.z;
    float refZ = refp.z - p0.z;
    gradient = vec2(
        edgeZ * toRef.y - refZ * edge.y,
        edge.x * refZ - toRef.x * edgeZ) / det;
    projectedEdge = edge;
    referenceSideCross = det;
    // The ordered comparison also rejects NaN; the magnitude guard rejects
    // infinities and pathological near-degenerate projections.
    return all(lessThan(abs(gradient), vec2(1.0e20)));
}

float boundedSupportingDepthCorrection(
    vec2 offsetNdc,
    vec2 depthGradient,
    vec2 projectedEdge,
    float referenceSideCross)
{
    float offsetSideCross =
        projectedEdge.x * offsetNdc.y - projectedEdge.y * offsetNdc.x;
    float referenceRatio = offsetSideCross / referenceSideCross;
    if (!(referenceRatio > 0.0))
    {
        return 0.0;
    }

    float scale = (referenceRatio > 1.0) ? (1.0 / referenceRatio) : 1.0;
    float correction = scale * dot(depthGradient, offsetNdc);
    return (abs(correction) < 1.0e20) ? correction : 0.0;
}

vec4 expandedSurfacePoint(
    vec4 clip,
    vec2 offsetNdc,
    vec2 depthGradient,
    vec2 projectedEdge,
    float referenceSideCross,
    bool matchSurface)
{
    float ndcDepth = clip.z / clip.w;
    if (matchSurface)
    {
        ndcDepth += boundedSupportingDepthCorrection(
            offsetNdc,
            depthGradient,
            projectedEdge,
            referenceSideCross);
    }
    ndcDepth -= max(u_ndc_z_bias, 0.0);

    vec4 expanded = clip;
    expanded.xy += offsetNdc * clip.w;
    expanded.z = ndcDepth * clip.w;
    return expanded;
}

void main()
{
    vec4 p0c = gl_in[0].gl_Position;
    vec4 p1c = gl_in[1].gl_Position;
    if (!clipLineToHomogeneousFrustum(p0c, p1c))
    {
        return;
    }

    vec2 p0 = p0c.xy / p0c.w;
    vec2 p1 = p1c.xy / p1c.w;

    vec2 d = (p1 - p0) * u_viewport_size;
    float len = length(d);
    if (len < 1e-4)
    {
        return;
    }

    vec2 dir = d / len;
    vec2 n = vec2(-dir.y, dir.x);

    float halfw = max(u_line_width_px, 1.0) * 0.5;
    vec2 offset_ndc = (n * halfw) * 2.0 / u_viewport_size;

    vec2 depthGradient = vec2(0.0);
    vec2 projectedEdge = vec2(0.0);
    float referenceSideCross = 0.0;
    bool matchSurface =
        (u_surface_depth_match != 0)
        && (v_plane_valid[0] > 0.5)
        && (v_plane_valid[1] > 0.5)
        && supportingSurfaceDepthGradient(
            p0c,
            p1c,
            v_plane_ref_clip[0],
            depthGradient,
            projectedEdge,
            referenceSideCross);

    // The bounded support correction is piecewise linear across the line
    // width: it is zero on the outward half, follows the supporting plane
    // inward, then remains at the reference boundary if that point lies inside
    // the quad. A single quad interpolates its four corner depths across all
    // three regions and can miss the filled surface at grazing angles. Emit a
    // continuous strip with a shared boundary at the edge and at the clamp
    // point so raster interpolation follows the same piecewise depth function.
    float bandOffsetsPixels[4];
    int bandBoundaryCount = 2;
    bandOffsetsPixels[0] = -halfw;
    bandOffsetsPixels[1] = halfw;
    if (matchSurface)
    {
        float sideCross =
            projectedEdge.x * offset_ndc.y - projectedEdge.y * offset_ndc.x;
        float referenceRatio = sideCross / referenceSideCross;
        if (!isnan(referenceRatio)
            && !isinf(referenceRatio)
            && (abs(referenceRatio) > 1.0e-20))
        {
            bandBoundaryCount = 3;
            bandOffsetsPixels[1] = 0.0;
            bandOffsetsPixels[2] = halfw;

            float supportBoundaryPixels =
                halfw / max(1.0, abs(referenceRatio));
            if ((supportBoundaryPixels > 0.0)
                && (supportBoundaryPixels < halfw))
            {
                if (referenceRatio > 0.0)
                {
                    bandOffsetsPixels[2] = supportBoundaryPixels;
                    bandOffsetsPixels[3] = halfw;
                }
                else
                {
                    bandOffsetsPixels[1] = -supportBoundaryPixels;
                    bandOffsetsPixels[2] = 0.0;
                    bandOffsetsPixels[3] = halfw;
                }
                bandBoundaryCount = 4;
            }
        }
        else
        {
            matchSurface = false;
        }
    }

    for (int boundary = 0; boundary < bandBoundaryCount; ++boundary)
    {
        vec2 boundaryOffsetNdc =
            offset_ndc * (bandOffsetsPixels[boundary] / halfw);
        gl_Position = expandedSurfacePoint(
            p0c,
            boundaryOffsetNdc,
            depthGradient,
            projectedEdge,
            referenceSideCross,
            matchSurface);
        EmitVertex();
        gl_Position = expandedSurfacePoint(
            p1c,
            boundaryOffsetNdc,
            depthGradient,
            projectedEdge,
            referenceSideCross,
            matchSurface);
        EmitVertex();
    }
    EndPrimitive();
}
)GLSL";

    const char* thickLineFrag = R"GLSL(
#version 330 core
uniform vec4 u_color;
out vec4 fragColor;
void main()
{
    fragColor = u_color;
}
)GLSL";

    if (!thickLineProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, thickLineVert)
        || !thickLineProgram_->addShaderFromSourceCode(QOpenGLShader::Geometry, thickLineGeom)
        || !thickLineProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, thickLineFrag)
        || !thickLineProgram_->link())
    {
        Q_EMIT warningMessage(QString("Failed to compile/link thick-line shader (selection lines will be thin): %1").arg(thickLineProgram_->log()));
        thickLineProgram_.reset();
    }
    else
    {
        thickLineProgram_->bind();
        thickLineProgram_->setUniformValue("u_line_width_px", 2.0f);
        thickLineProgram_->setUniformValue("u_viewport_size", QVector2D(1.0f, 1.0f));
        thickLineProgram_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        thickLineProgram_->setUniformValue("u_frame_xform_enabled", 0);
        thickLineProgram_->setUniformValue("u_frame_xform_id", 0);
        thickLineProgram_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
        thickLineProgram_->setUniformValue("u_xform_enabled", 0);
        thickLineProgram_->setUniformValue("u_xform_count", 0);
        thickLineProgram_->setUniformValue("u_xform_matrix", QMatrix4x4());
        thickLineProgram_->setUniformValue("u_ndc_z_bias", 0.0f);
        const render::WireframeOverlayDepthPolicy wireDepthPolicy =
            render::wireframeOverlayDepthPolicy();
        thickLineProgram_->setUniformValue(
            "u_surface_depth_match",
            wireDepthPolicy.matchSupportingSurfaceDepth ? 1 : 0);
        thickLineProgram_->setUniformValue(
            "u_plane_min_sin_squared",
            wireDepthPolicy.projectedPlaneMinSinSquared);
        thickLineProgram_->setUniformValue(
            "u_clip_min_w",
            wireDepthPolicy.homogeneousClipMinimumW);
        thickLineProgram_->release();
    }

    gridProgram_ = std::make_unique<QOpenGLShaderProgram>();
    const char* gridVert = R"GLSL(
#version 330 core
out vec2 v_ndc;

void main()
{
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    v_ndc = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

    const char* gridFrag = R"GLSL(
#version 330 core
in vec2 v_ndc;

uniform mat4 u_inv_mvp;
uniform mat4 u_mvp;
uniform vec3 u_camera_pos;
uniform vec3 u_plane_point;
uniform vec3 u_plane_normal;
uniform vec3 u_axis_a;
uniform vec3 u_axis_b;
uniform vec4 u_axis_a_clip_direction;
uniform vec4 u_axis_b_clip_direction;
uniform vec3 u_coord_origin;
uniform vec3 u_view_center;
uniform vec2 u_minor_coord_phase;
uniform vec2 u_major_coord_phase;
uniform vec2 u_major_lod1_coord_phase;
uniform vec2 u_major_lod2_coord_phase;
uniform vec2 u_major_lod3_coord_phase;
uniform int u_is_ortho;
uniform float u_minor_spacing;
uniform float u_major_spacing;
uniform float u_minor_visibility;
uniform float u_major_visibility;
uniform int u_minor_points_enabled;
uniform float u_minor_dot_diameter_px;
uniform float u_fade_radius;
uniform float u_fade_softness;
uniform vec2 u_clip_fade_ratios;
uniform vec2 u_minor_footprint_fade_px;
uniform vec2 u_major_footprint_fade_px;
uniform float u_line_width_px;
uniform float u_axis_line_width_px;
uniform vec2 u_viewport_size;
uniform vec3 u_axis_a_screen_line;
uniform vec3 u_axis_b_screen_line;
uniform float u_ndc_z_bias;
uniform vec4 u_minor_color;
uniform vec4 u_major_color;
uniform vec4 u_axis_a_color;
uniform vec4 u_axis_b_color;

out vec4 fragColor;

vec3 unprojectPoint(vec2 ndc, float ndcZ)
{
    vec4 p = u_inv_mvp * vec4(ndc, ndcZ, 1.0);
    float safeW = (abs(p.w) > 1.0e-6) ? p.w : ((p.w < 0.0) ? -1.0e-6 : 1.0e-6);
    return p.xyz / safeW;
}

float analyticLineCoverage(float distPx, float widthPx)
{
    float safeWidthPx = max(widthPx, 1.0e-4);
    float halfWidthPx = 0.5 * safeWidthPx;
    float plateau = min(safeWidthPx, 1.0);
    float transitionStart = abs(halfWidthPx - 0.5);
    float transitionEnd = halfWidthPx + 0.5;
    if (distPx <= transitionStart)
    {
        return plateau;
    }
    return plateau * clamp(
        (transitionEnd - distPx) / max(transitionEnd - transitionStart, 1.0e-5),
        0.0,
        1.0);
}

float coordinateDerivative(float coord)
{
    // fwidth() is an L1 norm and makes a 45-degree grid up to sqrt(2) softer
    // than an axis-aligned one. The Euclidean gradient keeps the handoff
    // invariant under viewport rotation.
    return max(length(vec2(dFdx(coord), dFdy(coord))), 1.0e-5);
}

float gridFootprintFadeFromDerivative(float cellsPerPixel, vec2 fadeBandPx, float widthPx)
{
    // cellsPerPixel is the number of grid cells covered by approximately one
    // physical framebuffer pixel. Fade each line direction independently well before its
    // period reaches the formal two-pixel Nyquist limit. A shared two-axis maximum makes one
    // foreshortened direction incorrectly erase the still-resolved direction
    // and leaves phase-sampled Moire during perspective camera motion.
    float pixelsPerCell = 1.0 / cellsPerPixel;
    // A thick line occupies more of every cell, so applying the one-pixel
    // thresholds unchanged lets it merge into dense bands well before the
    // formal sampling limit. Hand thick lines to the next LOD proportionally
    // earlier while preserving the exact requested width on resolved cells.
    vec2 widthAwareBandPx = fadeBandPx * max(widthPx, 1.0);
    return smoothstep(widthAwareBandPx.x, widthAwareBandPx.y, pixelsPerCell);
}

float gridPointFootprintFadeFromDerivative(
    float cellsPerPixel,
    float diameterPx,
    vec2 minimumFadeBandPx)
{
    float pixelsPerCell = 1.0 / cellsPerPixel;
    // Point coverage has radial support radius d/2 + 0.5 px, including the
    // one-pixel AA fringe. Once the period reaches that radius, any sample
    // inside a dot's support can move each lattice coordinate by less than
    // one cell; the fixed +/-1 index search is therefore complete. Keep the
    // existing minor-grid density thresholds as floors, but do not multiply
    // them by dot diameter as for continuous lines.
    float radialSupportRadiusPx = 0.5 * diameterPx + 0.5;
    vec2 pointFadeBandPx = max(
        minimumFadeBandPx,
        vec2(radialSupportRadiusPx, 4.0 * radialSupportRadiusPx));
    return smoothstep(pointFadeBandPx.x, pointFadeBandPx.y, pixelsPerCell);
}

vec3 projectedLineEquation(vec4 pointClip, vec4 directionClip)
{
    vec3 equation = cross(
        vec3(pointClip.x, pointClip.y, pointClip.w),
        vec3(directionClip.x, directionClip.y, directionClip.w));
    float normalLength = length(equation.xy);
    return (normalLength > 1.0e-8) ? (equation / normalLength) : vec3(0.0);
}

float screenLineDistancePx(vec3 equation)
{
    vec2 safeViewport = max(u_viewport_size, vec2(1.0));
    vec2 gradientPerPixel = equation.xy * (2.0 / safeViewport);
    float gradientLength = length(gradientPerPixel);
    if (gradientLength <= 1.0e-8)
    {
        return 1.0e20;
    }
    return abs(dot(equation, vec3(v_ndc, 1.0))) / gradientLength;
}

float projectedRepeatedLine(
    float localCoord,
    float phase,
    float spacing,
    float widthPx,
    vec2 fadeBandPx,
    vec4 hitClip,
    vec4 perpendicularClipDirection,
    vec4 alongClipDirection)
{
    // Map Settings permits a 1e-6 minor spacing. Keep the shader's safety
    // floor below that authored contract so rendering never substitutes a
    // coarser lattice than Move/Cleave snapping.
    float safeSpacing = max(spacing, 1.0e-9);
    float coord = (localCoord + phase) / safeSpacing;
    float cellsPerPixel = coordinateDerivative(coord);
    float footprint = gridFootprintFadeFromDerivative(cellsPerPixel, fadeBandPx, widthPx);
    if (footprint <= 1.0e-4 || abs(coord) > 8388608.0)
    {
        return 0.0;
    }

    // Coverage is deliberately independent of the coordinate derivative.
    // Near a projective pole a 2x2 derivative quad can span a huge rational
    // discontinuity; using that value as line width made one ordinary grid
    // line expand into the broad red/green-axis wedge. Project the selected
    // nearest lattice line itself, then measure its exact framebuffer-pixel
    // distance. The two clip-space axis directions are precomputed on the CPU,
    // so all ten minor/major evaluations need only one scaled add and one cross
    // product rather than another full MVP multiply.
    float nearestDelta = (round(coord) - coord) * safeSpacing;
    vec4 linePointClip = hitClip + perpendicularClipDirection * nearestDelta;
    vec3 equation = projectedLineEquation(linePointClip, alongClipDirection);
    return analyticLineCoverage(screenLineDistancePx(equation), widthPx) * footprint;
}

float projectedGridPointDistanceSquaredPx(
    vec4 hitClip,
    vec4 axisAClipDirection,
    vec4 axisBClipDirection,
    vec2 latticeDelta,
    vec2 sampleNdc)
{
    vec4 pointClip = hitClip
        + axisAClipDirection * latticeDelta.x
        + axisBClipDirection * latticeDelta.y;
    if (pointClip.w <= 1.0e-6)
    {
        return 1.0e20;
    }

    float pointDepthNdc = pointClip.z / pointClip.w;
    if (abs(pointDepthNdc) > 1.0)
    {
        return 1.0e20;
    }
    vec2 pointNdc = pointClip.xy / pointClip.w;
    if (any(isnan(pointNdc)) || any(isinf(pointNdc)))
    {
        return 1.0e20;
    }

    vec2 framebufferDelta =
        (pointNdc - sampleNdc) * (0.5 * u_viewport_size);
    return dot(framebufferDelta, framebufferDelta);
}

float projectedMinorGridPointCoverage(
    float localA,
    float localB,
    vec2 coordPhase,
    float spacing,
    float diameterPx,
    vec4 hitClip,
    vec4 axisAClipDirection,
    vec4 axisBClipDirection)
{
    float safeSpacing = max(spacing, 1.0e-9);
    vec2 coord = (vec2(localA, localB) + coordPhase) / safeSpacing;
    // A circular dot is useful only while both grid directions are resolved.
    // Taking the minimum prevents a well-spaced direction from hiding a
    // foreshortened direction at oblique perspective angles.
    float footprintA = gridPointFootprintFadeFromDerivative(
        coordinateDerivative(coord.x), diameterPx, u_minor_footprint_fade_px);
    float footprintB = gridPointFootprintFadeFromDerivative(
        coordinateDerivative(coord.y), diameterPx, u_minor_footprint_fade_px);
    float footprint = min(footprintA, footprintB);
    if (any(greaterThan(abs(coord), vec2(8388608.0)))
        || footprint <= 1.0e-4)
    {
        return 0.0;
    }

    // Independently rounding both plane coordinates can pick the wrong screen
    // neighbor when the projected axes are strongly skewed. Search a fixed
    // 3x3 set around that estimate and measure each true framebuffer distance.
    // The conservative two-direction footprint fade above keeps the search
    // bounded to nearby candidates as the grid approaches its sampling limit.
    vec2 nearestIndex = round(coord);
    float nearestDistanceSquaredPx = 1.0e20;
    for (int offsetA = -1; offsetA <= 1; ++offsetA)
    {
        for (int offsetB = -1; offsetB <= 1; ++offsetB)
        {
            vec2 latticeDelta =
                (nearestIndex + vec2(float(offsetA), float(offsetB)) - coord) * safeSpacing;
            nearestDistanceSquaredPx = min(
                nearestDistanceSquaredPx,
                projectedGridPointDistanceSquaredPx(
                    hitClip,
                    axisAClipDirection,
                    axisBClipDirection,
                    latticeDelta,
                    v_ndc));
        }
    }

    float nearestDistancePx = sqrt(max(nearestDistanceSquaredPx, 0.0));
    float radialCoverage = clamp(
        0.5 * diameterPx + 0.5 - nearestDistancePx,
        0.0,
        1.0);
    return radialCoverage * footprint;
}

float screenLineCoverage(vec3 equation, float widthPx)
{
    return analyticLineCoverage(screenLineDistancePx(equation), widthPx);
}

vec4 composite(vec4 dst, vec4 src)
{
    float outA = src.a + dst.a * (1.0 - src.a);
    if (outA <= 1.0e-6)
    {
        return vec4(0.0);
    }

    vec3 outRgb = (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / outA;
    return vec4(outRgb, outA);
}

vec4 replaceLineColor(vec4 dst, vec4 lineColor, float coverage)
{
    float srcA = clamp(lineColor.a * coverage, 0.0, 1.0);
    float outA = max(dst.a, srcA);
    if (outA <= 1.0e-6)
    {
        return vec4(0.0);
    }

    // The axis is a color replacement, not an alpha cut followed by a
    // source-over draw. Preserve the union/max coverage so a capped 4 px axis
    // cannot carve a dark AA moat into a 10 px ordinary line.
    float replacement = clamp(srcA / outA, 0.0, 1.0);
    return vec4(mix(dst.rgb, lineColor.rgb, replacement), outA);
}

void main()
{
    vec3 nearWorld = unprojectPoint(v_ndc, -1.0);
    vec3 farWorld = unprojectPoint(v_ndc, 1.0);
    vec3 rayOrigin = (u_is_ortho != 0) ? nearWorld : u_camera_pos;
    vec3 rayDir = farWorld - nearWorld;
    float rayLen = length(rayDir);
    if (rayLen <= 1.0e-6)
    {
        discard;
    }
    rayDir /= rayLen;

    float denom = dot(rayDir, u_plane_normal);
    float planeFacing = abs(denom);
    if (planeFacing < 1.0e-6)
    {
        discard;
    }

    float t = dot(u_plane_point - rayOrigin, u_plane_normal) / denom;
    if (t <= 0.0)
    {
        discard;
    }

    vec3 hit = rayOrigin + rayDir * t;
    vec4 hitClip = u_mvp * vec4(hit, 1.0);
    float hitW = (abs(hitClip.w) > 1.0e-6) ? hitClip.w : ((hitClip.w < 0.0) ? -1.0e-6 : 1.0e-6);
    float hitDepthNdc = hitClip.z / hitW;
    float hitDepth01 = hitDepthNdc * 0.5 + 0.5;
    if (hitDepthNdc < -1.0 || hitDepth01 > 1.0005)
    {
        discard;
    }

    // NDC depth is deliberately not used for fading: perspective depth is
    // highly non-linear and made ordinary grid distances noticeably dimmer
    // than the same view in orthographic projection.  Measure against the far
    // point on this reconstructed ray instead, retaining only a narrow fade
    // immediately before the hard clip plane.
    float rayFarDistance = length(farWorld - rayOrigin);
    float clipRatio = (rayFarDistance > 1.0e-6)
        ? clamp(t / rayFarDistance, 0.0, 1.0)
        : 0.0;
    float clipFade = 1.0 - smoothstep(
        u_clip_fade_ratios.x,
        u_clip_fade_ratios.y,
        clipRatio);

    // Work camera-relative, then add a small wrapped phase for the actual grid
    // origin. This avoids `fract()` on huge world-space coordinates, which is
    // unstable on GPUs when editing levels far from (0,0,0).
    vec3 relToViewCenter = hit - u_view_center;
    float localA = dot(relToViewCenter, u_axis_a);
    float localB = dot(relToViewCenter, u_axis_b);

    float minorSpacing = max(u_minor_spacing, 1.0e-9);
    float majorSpacing = max(u_major_spacing, minorSpacing);

    // A-coordinate lines run along B; B-coordinate lines run along A.
    float minorA = 0.0;
    float minorB = 0.0;
    float minorPoint = 0.0;
    if (u_minor_points_enabled != 0)
    {
        minorPoint = projectedMinorGridPointCoverage(
            localA,
            localB,
            u_minor_coord_phase,
            minorSpacing,
            u_minor_dot_diameter_px,
            hitClip,
            u_axis_a_clip_direction,
            u_axis_b_clip_direction);
    }
    else
    {
        minorA = projectedRepeatedLine(
            localA, u_minor_coord_phase.x, minorSpacing, u_line_width_px,
            u_minor_footprint_fade_px, hitClip, u_axis_a_clip_direction, u_axis_b_clip_direction);
        minorB = projectedRepeatedLine(
            localB, u_minor_coord_phase.y, minorSpacing, u_line_width_px,
            u_minor_footprint_fade_px, hitClip, u_axis_b_clip_direction, u_axis_a_clip_direction);
    }
    // Keep four exact 10x major-grid levels available. At a distance the
    // screen-footprint filter removes a dense level before it becomes phase
    // unstable, while its coarser subset remains. This is the important part
    // of Blender-like grid stability; merely lowering alpha still aliases.
    float majorA0 = projectedRepeatedLine(
        localA, u_major_coord_phase.x, majorSpacing, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_a_clip_direction, u_axis_b_clip_direction);
    float majorB0 = projectedRepeatedLine(
        localB, u_major_coord_phase.y, majorSpacing, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_b_clip_direction, u_axis_a_clip_direction);
    float majorA1 = projectedRepeatedLine(
        localA, u_major_lod1_coord_phase.x, majorSpacing * 10.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_a_clip_direction, u_axis_b_clip_direction);
    float majorB1 = projectedRepeatedLine(
        localB, u_major_lod1_coord_phase.y, majorSpacing * 10.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_b_clip_direction, u_axis_a_clip_direction);
    float majorA2 = projectedRepeatedLine(
        localA, u_major_lod2_coord_phase.x, majorSpacing * 100.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_a_clip_direction, u_axis_b_clip_direction);
    float majorB2 = projectedRepeatedLine(
        localB, u_major_lod2_coord_phase.y, majorSpacing * 100.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_b_clip_direction, u_axis_a_clip_direction);
    float majorA3 = projectedRepeatedLine(
        localA, u_major_lod3_coord_phase.x, majorSpacing * 1000.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_a_clip_direction, u_axis_b_clip_direction);
    float majorB3 = projectedRepeatedLine(
        localB, u_major_lod3_coord_phase.y, majorSpacing * 1000.0, u_line_width_px, u_major_footprint_fade_px,
        hitClip, u_axis_b_clip_direction, u_axis_a_clip_direction);
    float majorA = max(max(majorA0, majorA1), max(majorA2, majorA3));
    float majorB = max(max(majorB0, majorB1), max(majorB2, majorB3));

    float minorAngleFade = (u_is_ortho != 0) ? 1.0 : smoothstep(0.035, 0.18, planeFacing);
    float majorAngleFade = (u_is_ortho != 0) ? 1.0 : smoothstep(0.008, 0.09, planeFacing);
    float minorLine = max(minorA, minorB) * clamp(u_minor_visibility, 0.0, 1.0) * minorAngleFade;
    minorPoint *= clamp(u_minor_visibility, 0.0, 1.0) * minorAngleFade;
    float majorLine = max(majorA, majorB) * clamp(u_major_visibility, 0.0, 1.0) * majorAngleFade;
    minorLine *= (1.0 - majorLine);
    minorPoint *= (1.0 - majorLine);
    // The old implicit axis used `local + absoluteViewOffset`. At large world
    // coordinates that subtraction quantized a whole screen band to exactly
    // zero, so fwidth filtering painted it as a glow. Project each infinite
    // axis to a homogeneous screen line on the CPU and measure true pixel
    // distance here; no large world-coordinate cancellation remains.
    float axisALine = screenLineCoverage(u_axis_a_screen_line, u_axis_line_width_px);
    float axisBLine = screenLineCoverage(u_axis_b_screen_line, u_axis_line_width_px);
    float fadeSoftness = max(u_fade_softness, 1.0e-4);
    float fade =
        (1.0 - smoothstep(u_fade_radius, u_fade_radius + fadeSoftness, length(relToViewCenter)))
        * clipFade;
    if (fade <= 1.0e-4)
    {
        discard;
    }

    vec4 color = vec4(0.0);
    color = composite(color, vec4(u_minor_color.rgb, u_minor_color.a * minorLine));
    color = composite(color, vec4(u_minor_color.rgb, u_minor_color.a * minorPoint));
    color = composite(color, vec4(u_major_color.rgb, u_major_color.a * majorLine));
    color = replaceLineColor(color, u_axis_a_color, axisALine);
    color = replaceLineColor(color, u_axis_b_color, axisBLine);

    color.a *= fade;
    if (color.a <= 1.0e-4)
    {
        discard;
    }

    gl_FragDepth = clamp((hitDepthNdc - max(u_ndc_z_bias, 0.0)) * 0.5 + 0.5, 0.0, 0.999999);
    fragColor = color;
}
)GLSL";

    if (!gridProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, gridVert)
        || !gridProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, gridFrag)
        || !gridProgram_->link())
    {
        Q_EMIT warningMessage(QString("Failed to compile/link perspective grid shader: %1").arg(gridProgram_->log()));
        gridProgram_.reset();
    }
    else
    {
        gridProgram_->bind();
        gridProgram_->setUniformValue("u_inv_mvp", QMatrix4x4());
        gridProgram_->setUniformValue("u_mvp", QMatrix4x4());
        gridProgram_->setUniformValue("u_camera_pos", QVector3D(0.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_plane_point", QVector3D(0.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_plane_normal", QVector3D(0.0f, 0.0f, 1.0f));
        gridProgram_->setUniformValue("u_axis_a", QVector3D(1.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_axis_b", QVector3D(0.0f, 1.0f, 0.0f));
        gridProgram_->setUniformValue("u_axis_a_clip_direction", QVector4D(1.0f, 0.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_axis_b_clip_direction", QVector4D(0.0f, 1.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_coord_origin", QVector3D(0.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_view_center", QVector3D(0.0f, 0.0f, 0.0f));
        gridProgram_->setUniformValue("u_minor_coord_phase", QVector2D(0.0f, 0.0f));
        gridProgram_->setUniformValue("u_major_coord_phase", QVector2D(0.0f, 0.0f));
        gridProgram_->setUniformValue("u_major_lod1_coord_phase", QVector2D(0.0f, 0.0f));
        gridProgram_->setUniformValue("u_major_lod2_coord_phase", QVector2D(0.0f, 0.0f));
        gridProgram_->setUniformValue("u_major_lod3_coord_phase", QVector2D(0.0f, 0.0f));
        gridProgram_->setUniformValue("u_is_ortho", 0);
        gridProgram_->setUniformValue("u_minor_spacing", 0.1f);
        gridProgram_->setUniformValue("u_major_spacing", 1.0f);
        gridProgram_->setUniformValue("u_minor_visibility", 1.0f);
        gridProgram_->setUniformValue("u_major_visibility", 1.0f);
        gridProgram_->setUniformValue("u_fade_radius", 64.0f);
        gridProgram_->setUniformValue("u_fade_softness", 64.0f);
        const render::GridClipFadePolicy clipFadePolicy = render::gridClipFadePolicy();
        gridProgram_->setUniformValue(
            "u_clip_fade_ratios",
            QVector2D(clipFadePolicy.startRatio, clipFadePolicy.endRatio));
        const render::GridFootprintFadePolicy footprintFadePolicy = render::gridFootprintFadePolicy();
        gridProgram_->setUniformValue(
            "u_minor_footprint_fade_px",
            QVector2D(
                footprintFadePolicy.minorHiddenPixelsPerCell,
                footprintFadePolicy.minorFullyVisiblePixelsPerCell));
        gridProgram_->setUniformValue(
            "u_major_footprint_fade_px",
            QVector2D(
                footprintFadePolicy.majorHiddenPixelsPerCell,
                footprintFadePolicy.majorFullyVisiblePixelsPerCell));
        gridProgram_->setUniformValue("u_line_width_px", 1.0f);
        gridProgram_->setUniformValue("u_axis_line_width_px", 1.0f);
        gridProgram_->setUniformValue("u_viewport_size", QVector2D(1.0f, 1.0f));
        gridProgram_->setUniformValue("u_axis_a_screen_line", QVector3D());
        gridProgram_->setUniformValue("u_axis_b_screen_line", QVector3D());
        gridProgram_->setUniformValue("u_ndc_z_bias", 0.0f);
        gridProgram_->setUniformValue("u_minor_color", QVector4D(1.0f, 1.0f, 1.0f, 0.12f));
        gridProgram_->setUniformValue("u_major_color", QVector4D(1.0f, 1.0f, 1.0f, 0.22f));
        gridProgram_->setUniformValue("u_axis_a_color", QVector4D(1.0f, 0.0f, 0.0f, 0.58f));
        gridProgram_->setUniformValue("u_axis_b_color", QVector4D(0.0f, 1.0f, 0.0f, 0.58f));
        gridProgram_->release();
    }

    pickProgram_ = std::make_unique<QOpenGLShaderProgram>();
    // Note: we intentionally avoid uint/int varyings and bit-ops here.
    // Some drivers are picky about integer varyings; float IDs are plenty for our expected pick-id range.
    const char* pickVert = R"GLSL(
#version 330 core
 layout(location = 0) in vec3 a_pos;
 layout(location = 3) in float a_pick_id;
 layout(location = 4) in float a_pick_geo;
 uniform mat4 u_mvp;
 uniform int u_frame_xform_enabled;
 uniform int u_frame_xform_id;
 uniform mat4 u_frame_xform_matrix;
 uniform int u_xform_enabled;
 uniform int u_xform_count;
 uniform int u_xform_ids[256];
 uniform mat4 u_xform_matrix;
 flat out float v_id;
 flat out float v_geo;
 void main() {
  vec3 worldPos = a_pos;
  int pid = int(a_pick_id + 0.5);
  if ((u_frame_xform_enabled != 0) && (pid == u_frame_xform_id))
  {
    vec4 t = u_frame_xform_matrix * vec4(worldPos, 1.0);
    worldPos = t.xyz;
  }
  if (u_xform_enabled != 0)
  {
    for (int i = 0; i < u_xform_count; ++i)
    {
      if (pid == u_xform_ids[i])
      {
        worldPos = (u_xform_matrix * vec4(worldPos, 1.0)).xyz;
        break;
      }
    }
  }
  gl_Position = u_mvp * vec4(worldPos, 1.0);
  v_id = floor(a_pick_id + 0.5);
  v_geo = floor(a_pick_geo + 0.5);
 }
)GLSL";

    const char* pickFrag = R"GLSL(
#version 330 core
uniform int u_geo_min;
uniform int u_exclude_count;
uniform int u_exclude_ids[16];
out vec4 fragColor;

flat in float v_id;
flat in float v_geo;

vec4 encode_id(float id)
{
  float r = mod(id, 256.0);
  id = floor(id / 256.0);
  float g = mod(id, 256.0);
  id = floor(id / 256.0);
  float b = mod(id, 256.0);
  id = floor(id / 256.0);
  float a = mod(id, 256.0);
  return vec4(r, g, b, a) / 255.0;
}

void main() {
  if (v_id < 0.5) {
    discard;
  }
  if (v_geo < float(u_geo_min)) {
    discard;
  }
  for (int i = 0; i < u_exclude_count; ++i) {
    if (abs(v_id - float(u_exclude_ids[i])) < 0.5) {
      discard;
    }
  }
  fragColor = encode_id(v_id);
}
)GLSL";

    if (!pickProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, pickVert)
        || !pickProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, pickFrag) || !pickProgram_->link())
    {
        Q_EMIT errorMessage(QString("Failed to compile/link pick shader: %1").arg(pickProgram_->log()));
        pickProgram_.reset();
    }
    else
    {
        pickProgram_->bind();
        pickProgram_->setUniformValue("u_frame_xform_enabled", 0);
        pickProgram_->setUniformValue("u_frame_xform_id", 0);
        pickProgram_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
        pickProgram_->setUniformValue("u_xform_enabled", 0);
        pickProgram_->setUniformValue("u_xform_count", 0);
        pickProgram_->setUniformValue("u_xform_matrix", QMatrix4x4());
        pickProgram_->release();
    }

    triVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    triVao_->create();

    triVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    triVbo_->create();
    triVbo_->setUsagePattern(QOpenGLBuffer::StaticDraw);

    translucentTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    translucentTriVao_->create();

    translucentTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    translucentTriVbo_->create();
    translucentTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    thingGizmoTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    thingGizmoTriVao_->create();

    thingGizmoTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    thingGizmoTriVbo_->create();
    thingGizmoTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    thingGizmoPickTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    thingGizmoPickTriVao_->create();

    thingGizmoPickTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    thingGizmoPickTriVbo_->create();
    thingGizmoPickTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    lightGizmoTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    lightGizmoTriVao_->create();

    lightGizmoTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    lightGizmoTriVbo_->create();
    lightGizmoTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    spriteTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    spriteTriVao_->create();

    spriteTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    spriteTriVbo_->create();
    spriteTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    outlineTriVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    outlineTriVao_->create();

    outlineTriVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    outlineTriVbo_->create();
    outlineTriVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    lineVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    lineVao_->create();

    lineVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    lineVbo_->create();
    lineVbo_->setUsagePattern(QOpenGLBuffer::StaticDraw);

    highlightLineVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    highlightLineVao_->create();

    highlightLineVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    highlightLineVbo_->create();
    highlightLineVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    gridQuadVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    gridQuadVao_->create();

    gridLineVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    gridLineVao_->create();

    gridLineVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    gridLineVbo_->create();
    gridLineVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    pointVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    pointVao_->create();

    pointVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    pointVbo_->create();
    pointVbo_->setUsagePattern(QOpenGLBuffer::StaticDraw);

    highlightPointVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    highlightPointVao_->create();

    highlightPointVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    highlightPointVbo_->create();
    highlightPointVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    architecturalLightingPointVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    architecturalLightingPointVao_->create();

    architecturalLightingPointVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    architecturalLightingPointVbo_->create();
    architecturalLightingPointVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    transformGizmoLineVao_ = std::make_unique<QOpenGLVertexArrayObject>();
    transformGizmoLineVao_->create();

    transformGizmoLineVbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    transformGizmoLineVbo_->create();
    transformGizmoLineVbo_->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    if (gridLineVao_ && gridLineVbo_)
    {
        gridLineVao_->bind();
        gridLineVbo_->bind();
        gridLineVbo_->allocate(nullptr, 0);
        gridLineVboAllocatedBytes_ = 0;

        program_->bind();
        program_->enableAttributeArray(0);
        program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(LineVertex, x)), 3, static_cast<int>(sizeof(LineVertex)));
        program_->enableAttributeArray(3);
        program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(LineVertex, pickId)), 1, static_cast<int>(sizeof(LineVertex)));
        program_->release();

        gridLineVbo_->release();
        gridLineVao_->release();
    }

    transformGizmoLineVao_->bind();
    transformGizmoLineVbo_->bind();
    transformGizmoLineVbo_->allocate(nullptr, 0);

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
    program_->release();

    transformGizmoLineVbo_->release();
    transformGizmoLineVao_->release();

    if (thingGizmoTriVao_ && thingGizmoTriVbo_)
    {
        thingGizmoTriVao_->bind();
        thingGizmoTriVbo_->bind();
        thingGizmoTriVbo_->allocate(nullptr, 0);

        program_->bind();
        program_->enableAttributeArray(0);
        program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(1);
        program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(2);
        program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(3);
        program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(4);
        program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(5);
        program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(6);
        program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));
        program_->release();

        thingGizmoTriVbo_->release();
        thingGizmoTriVao_->release();
    }

    if (thingGizmoPickTriVao_ && thingGizmoPickTriVbo_)
    {
        thingGizmoPickTriVao_->bind();
        thingGizmoPickTriVbo_->bind();
        thingGizmoPickTriVbo_->allocate(nullptr, 0);

        program_->bind();
        program_->enableAttributeArray(0);
        program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(1);
        program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(2);
        program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(3);
        program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(4);
        program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(5);
        program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));
        program_->enableAttributeArray(6);
        program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));
        program_->release();

        thingGizmoPickTriVbo_->release();
        thingGizmoPickTriVao_->release();
    }

    if (lightGizmoTriVao_ && lightGizmoTriVbo_)
    {
        lightGizmoTriVao_->bind();
        lightGizmoTriVbo_->bind();
        lightGizmoTriVbo_->allocate(nullptr, 0);

        program_->bind();
        program_->enableAttributeArray(0);
        program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(1);
        program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(2);
        program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(3);
        program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(4);
        program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(5);
        program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(6);
        program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));
        program_->release();

        lightGizmoTriVbo_->release();
        lightGizmoTriVao_->release();
    }

    triVboAllocatedBytes_ = 0;
    lineVboAllocatedBytes_ = 0;
    pointVboAllocatedBytes_ = 0;
    translucentTriVboAllocatedBytes_ = 0;
    spriteTriVboAllocatedBytes_ = 0;
    highlightLineVboAllocatedBytes_ = 0;
    outlineTriVboAllocatedBytes_ = 0;
    highlightPointVboAllocatedBytes_ = 0;
    thingGizmoTriVboAllocatedBytes_ = 0;
    thingGizmoPickTriVboAllocatedBytes_ = 0;
    // A recreated GL context has an empty dedicated type-gizmo VBO even when
    // its CPU metadata survived on the widget.
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    lightGizmoTriVboAllocatedBytes_ = 0;
    transformGizmoLineVboAllocatedBytes_ = 0;
    gridLineVboAllocatedBytes_ = 0;

    initializeFrameGhostOpenGlResources();
    glReady_ = true;
    uploadMeshes();
}

void GlView::resizeGL(int w, int h)
{
    auto* f = QOpenGLContext::currentContext()->functions();
    f->glViewport(0, 0, w, h);
    markViewChanged();
}

void GlView::buildCameraMatrices(
    QMatrix4x4& proj,
    QMatrix4x4& view,
    QVector3D& eye,
    QVector3D& target,
    float& nearPlane,
    float& farPlane) noexcept
{
    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));

    camera_.SetCanvas(pixelW, pixelH);

    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
    camera_.UpdateFromOrbit();

    view = camera_.viewMatrix();
    eye = camera_.eye();
    target = camera_.target();
    nearPlane = camera_.nearPlane();
    farPlane = camera_.farPlane();

    // Orthographic depth is linear, so the persisted broad clip defaults
    // (±10,000 units) waste most of the 24-bit depth buffer on empty space.
    // Tighten only the projection used by the view and only to the currently
    // emitted level/Thing mesh bounds. Keep the authored planes in camera_ so
    // the viewport controls and saved SED state remain unchanged; intersect
    // the derived interval with them so an explicit user clip still wins.
    // Frame paths are render-only overlays whose authored points can lie
    // outside the Thing mesh bounds. Preserve the full authored range while
    // editing them instead of scanning every path on each camera/pick query.
    const bool useEffectiveOrthoRange =
        camera_.clipEnabled()
        && camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic
        && !frameEditEnabled_
        // The isolated authoring view explicitly publishes this resource
        // marker. Its fit already includes the sprite and displayed Bounds;
        // ordinary mesh caches omit camera-facing SPR offsets and would clip
        // that fitted card again. Main viewport depth tightening is unchanged.
        && !property("spr.preview.serialized_resource").isValid();
    if (useEffectiveOrthoRange)
    {
        QVector3D sceneMin{};
        QVector3D sceneMax{};
        bool haveSceneBounds = false;
        const auto includeBounds = [&sceneMin, &sceneMax, &haveSceneBounds](
                                       const MeshPartCache& cache)
        {
            if (!cache.valid || !cache.haveAny
                || !std::isfinite(cache.min.x()) || !std::isfinite(cache.min.y())
                || !std::isfinite(cache.min.z()) || !std::isfinite(cache.max.x())
                || !std::isfinite(cache.max.y()) || !std::isfinite(cache.max.z()))
            {
                return;
            }
            if (!haveSceneBounds)
            {
                sceneMin = cache.min;
                sceneMax = cache.max;
                haveSceneBounds = true;
                return;
            }
            sceneMin.setX(std::min(sceneMin.x(), cache.min.x()));
            sceneMin.setY(std::min(sceneMin.y(), cache.min.y()));
            sceneMin.setZ(std::min(sceneMin.z(), cache.min.z()));
            sceneMax.setX(std::max(sceneMax.x(), cache.max.x()));
            sceneMax.setY(std::max(sceneMax.y(), cache.max.y()));
            sceneMax.setZ(std::max(sceneMax.z(), cache.max.z()));
        };
        includeBounds(levelMeshCache_);
        includeBounds(thingMeshCache_);

        if (haveSceneBounds)
        {
            float minViewZ = std::numeric_limits<float>::max();
            float maxViewZ = -std::numeric_limits<float>::max();
            for (unsigned int corner = 0; corner < 8; ++corner)
            {
                const QVector3D worldPoint(
                    (corner & 1u) != 0u ? sceneMax.x() : sceneMin.x(),
                    (corner & 2u) != 0u ? sceneMax.y() : sceneMin.y(),
                    (corner & 4u) != 0u ? sceneMax.z() : sceneMin.z());
                const float viewZ = view.map(worldPoint).z();
                if (std::isfinite(viewZ))
                {
                    minViewZ = std::min(minViewZ, viewZ);
                    maxViewZ = std::max(maxViewZ, viewZ);
                }
            }

            bool canTightenRange = std::isfinite(minViewZ) && std::isfinite(maxViewZ)
                && maxViewZ >= minViewZ;

            // The occluded grid is an infinite plane. Include its full visible
            // orthographic footprint in the range; otherwise a custom grid
            // plane can be clipped even when the level itself is in range.
            // Always-behind and through-wall grids keep the authored projection
            // below, since those passes do not compare/write scene depth.
            if (canTightenRange && showGrid_
                && !gridAlwaysBehind_ && !gridVisibleThroughWalls_)
            {
                QVector3D axisA{ 1.0f, 0.0f, 0.0f };
                QVector3D axisB{ 0.0f, 1.0f, 0.0f };
                if (gridAlignToView_)
                {
                    QVector3D normal{};
                    if (!render::orthonormalizeGridAxes(
                            gridAlignAxisA_, gridAlignAxisB_, axisA, axisB, normal))
                    {
                        axisA = QVector3D(1.0f, 0.0f, 0.0f);
                        axisB = QVector3D(0.0f, 1.0f, 0.0f);
                    }
                }
                else if (gridRotateWithView_)
                {
                    bool inverseViewValid = false;
                    const QMatrix4x4 inverseView = view.inverted(&inverseViewValid);
                    if (inverseViewValid)
                    {
                        const QVector4D right4 = inverseView.column(0);
                        const QVector4D up4 = inverseView.column(1);
                        const render::ViewAlignedGridFrame frame = render::makeViewAlignedGridFrame(
                            QVector3D(right4.x(), right4.y(), right4.z()),
                            QVector3D(up4.x(), up4.y(), up4.z()),
                            target - eye);
                        if (frame.valid)
                        {
                            axisA = frame.axisA;
                            axisB = frame.axisB;
                        }
                    }
                }
                else
                {
                    switch (gridPlane_)
                    {
                    case GridPlane::XY:
                        axisA = QVector3D(1.0f, 0.0f, 0.0f);
                        axisB = QVector3D(0.0f, 1.0f, 0.0f);
                        break;
                    case GridPlane::XZ:
                        axisA = QVector3D(1.0f, 0.0f, 0.0f);
                        axisB = QVector3D(0.0f, 0.0f, 1.0f);
                        break;
                    case GridPlane::YZ:
                        axisA = QVector3D(0.0f, 1.0f, 0.0f);
                        axisB = QVector3D(0.0f, 0.0f, 1.0f);
                        break;
                    }
                }

                QVector3D gridCenter = gridCenterOverride_.value_or(
                    gridFollowCamera_ ? target : QVector3D{});
                QVector3D gridPlanePoint = gridCenter;
                if (!gridCenterOverride_.has_value()
                    && !gridAlignToView_ && !gridRotateWithView_)
                {
                    gridPlanePoint = axisA * QVector3D::dotProduct(gridCenter, axisA)
                        + axisB * QVector3D::dotProduct(gridCenter, axisB);
                }
                else if (gridRotateWithView_ && gridKeepViewAlignedVisible_)
                {
                    gridPlanePoint = render::resolveViewAlignedGridDisplayPlane(
                        gridCenter,
                        eye,
                        target - eye,
                        nearPlane,
                        true).planePoint;
                }

                QVector3D gridNormal = QVector3D::crossProduct(axisA, axisB);
                QVector3D forward = target - eye;
                if (!normalizeInPlace(gridNormal) || !normalizeInPlace(forward))
                {
                    canTightenRange = false;
                }
                else
                {
                    const float rayPlaneDot = QVector3D::dotProduct(gridNormal, forward);
                    if (std::abs(rayPlaneDot) <= 1.0e-3f)
                    {
                        // A near-edge-on infinite plane has no bounded viewport
                        // footprint. Retain authored clipping rather than make
                        // part of that user-selected grid disappear.
                        canTightenRange = false;
                    }
                    else
                    {
                        bool inverseViewValid = false;
                        const QMatrix4x4 inverseView = view.inverted(&inverseViewValid);
                        const QVector4D right4 = inverseViewValid
                            ? inverseView.column(0) : QVector4D(1.0f, 0.0f, 0.0f, 0.0f);
                        const QVector4D up4 = inverseViewValid
                            ? inverseView.column(1) : QVector4D(0.0f, 1.0f, 0.0f, 0.0f);
                        const QVector3D right(right4.x(), right4.y(), right4.z());
                        const QVector3D up(up4.x(), up4.y(), up4.z());
                        const float halfWidth = std::abs(camera_.orthoRight() - camera_.orthoLeft()) * 0.5f;
                        const float halfHeight = std::abs(camera_.orthoTop() - camera_.orthoBottom()) * 0.5f;
                        const float gridDepthRadius = (
                            std::abs(QVector3D::dotProduct(gridNormal, right)) * halfWidth
                            + std::abs(QVector3D::dotProduct(gridNormal, up)) * halfHeight)
                            / std::abs(rayPlaneDot);
                        const float centerDistance = QVector3D::dotProduct(
                            gridNormal, gridPlanePoint - eye) / rayPlaneDot;
                        const float gridCenterViewZ = view.map(eye + forward * centerDistance).z();
                        if (std::isfinite(gridDepthRadius) && std::isfinite(gridCenterViewZ))
                        {
                            minViewZ = std::min(minViewZ, gridCenterViewZ - gridDepthRadius);
                            maxViewZ = std::max(maxViewZ, gridCenterViewZ + gridDepthRadius);
                        }
                    }
                }
            }

            if (canTightenRange)
            {
                const float viewDepthSpan = maxViewZ - minViewZ;
                const float overlayMargin = std::max({
                    1.0f,
                    viewDepthSpan * 0.08f,
                    thingGizmoWorldSize_ * 4.0f,
                    lightGizmoWorldSize_ * 4.0f,
                });
                const float derivedNear = -maxViewZ - overlayMargin;
                const float derivedFar = -minViewZ + overlayMargin;
                // Orthographic ViewportCamera near planes are stored as
                // non-positive view-space clip coordinates. SetNearPlane()
                // applies this same cap; mirror it here so callers using the
                // returned scalar planes agree with the copied projection.
                const float effectiveNear = std::min(
                    std::max(camera_.nearPlane(), derivedNear), 0.0f);
                const float effectiveFar = std::min(camera_.farPlane(), derivedFar);
                if (std::isfinite(effectiveNear) && std::isfinite(effectiveFar)
                    && effectiveFar > effectiveNear + 0.01f)
                {
                    nearPlane = effectiveNear;
                    farPlane = effectiveFar;
                }
            }
        }
    }

    proj = camera_.projectionMatrix();
    if (camera_.clipEnabled()
        && camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic
        && (nearPlane != camera_.nearPlane() || farPlane != camera_.farPlane()))
    {
        app::ViewportCamera effectiveCamera = camera_;
        effectiveCamera.SetNearFar(nearPlane, farPlane);
        effectiveCamera.Update();
        proj = effectiveCamera.projectionMatrix();
    }
}

void GlView::paintGL()
{
    stagedCleaveTransformHudRect_ = {};
    // A camera or Level Settings edit can cross a 3DO geoset threshold without
    // changing authored geometry. Refresh before pass availability and batch
    // ranges are derived, while this widget's GL context is current.
    (void)refreshThingModelLodForView();
    auto* f = QOpenGLContext::currentContext()->functions();

    // Step 1: size the OpenGL viewport in physical pixels and clear the frame.
    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    f->glViewport(0, 0, pixelW, pixelH);

    // Reset key GL state each frame: Qt and our picking/overlay passes can leave masks off,
    // which makes clears and depth behavior nondeterministic.
    f->glDisable(GL_BLEND);
    f->glDisable(GL_CULL_FACE);
    f->glDisable(GL_POLYGON_OFFSET_FILL);
    f->glDisable(GL_SCISSOR_TEST);
    f->glDisable(GL_STENCIL_TEST);
    // Scene multisampling follows the requested framebuffer format. The
    // Wireframe AA preference is scoped to line overlays below.
    f->glEnable(GL_MULTISAMPLE);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LESS);
    f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    f->glDepthMask(GL_TRUE);
    f->glStencilMask(0xFF);
    f->glClearColor(
        backgroundClearColor_.redF(),
        backgroundClearColor_.greenF(),
        backgroundClearColor_.blueF(),
        1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Step 2: derive pass availability from user-visible toggles and uploaded buffer state.
    const bool wireframeBaseMode = !showSolid_ && !showVertices_;
    const bool canDrawLevelLines = lineVao_ && wireLineVertexCount_ > 0 && (wireframeBaseMode || showWireframe_);
    const bool canDrawThingLines = lineVao_ && thingLineVertexCount_ > 0;
    const bool canDrawThingCollisionOverlayLines =
        lineVao_ && showThingCollisionShapes_ && thingOverlayLineVertexCount_ > 0
        && !thingCollisionOverlayLineRanges_.empty();
    const bool canDrawThingBroadPhaseTreeGuideLines =
        lineVao_ && showThingCollisionShapes_ && thingOverlayLineVertexCount_ > 0
        && !thingBroadPhaseTreeGuideLineRanges_.empty();
    const bool canDrawThingVisualBoundsOverlayLines =
        lineVao_ && showThingVisualBounds_ && thingOverlayLineVertexCount_ > 0
        && !thingVisualBoundsOverlayLineRanges_.empty();
    const bool canDrawTemplateCollisionBoundsGuideLines =
        lineVao_ && renderPipeline_ == RenderPipeline::GamePreview
        && showTemplateCollisionBoundsGuide_ && thingOverlayLineVertexCount_ > 0
        && !thingTemplateCollisionBoundsGuideLineRanges_.empty();
    const bool canDrawThingOverlayLines =
        canDrawThingCollisionOverlayLines || canDrawThingBroadPhaseTreeGuideLines
        || canDrawThingVisualBoundsOverlayLines || canDrawTemplateCollisionBoundsGuideLines;
    const bool canDrawHighlightLines = highlightLineVao_ && highlightLineVertexCount_ > 0;
    const bool canDrawAnyLines = canDrawLevelLines || canDrawThingLines
        || canDrawThingOverlayLines || canDrawHighlightLines;

    const bool forceLevelPoints = renderPipeline_ == RenderPipeline::Editor
        && surfaceSubMode_ == SurfaceSubMode::Vertex
        && pickMode_ == PickMode::Surface;
    const bool canDrawLevelPoints = (showVertices_ || forceLevelPoints) && pointVao_ && levelPointVertexCount_ > 0;
    const bool canDrawThingPoints =
        (thingViewMode_ == ThingViewMode::Dots
            || thingViewMode_ == ThingViewMode::Vertices)
        && pointVao_ && thingPointVertexCount_ > 0;
    const bool canDrawHighlightPoints = highlightPointVao_ && highlightPointVertexCount_ > 0;
    const bool canDrawArchitecturalLightingPoints =
        renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingEditingActive_
        && architecturalLightingPointVao_
        && architecturalLightingPointCount_ > 0;
    const bool canDrawPoints = canDrawLevelPoints || canDrawThingPoints
        || canDrawHighlightPoints || canDrawArchitecturalLightingPoints;

    const bool canDrawLevelSolid = showSolid_;
    const bool forcePlacementThingSolid = placementPreviewThing_.has_value() || !placementPreviewThings_.empty();
    const bool canDrawThingSolid =
        (showThings_ || forcePlacementThingSolid)
        && (thingViewMode_ == ThingViewMode::Shaded || forcePlacementThingSolid);
    // Sprites use their own vertex buffer. Do not require an unrelated level
    // or 3DO triangle to admit the solid pass: an isolated sprite preview may
    // contain only sprite geometry. The Thing visibility/view-mode policy
    // still controls whether that buffer can be drawn.
    const bool canDrawSpriteSolid =
        canDrawThingSolid && spriteTriVao_ && spriteTriVertexCount_ > 0;
    const bool canDrawSolid =
        (triVao_ && triVertexCount_ > 0 && (canDrawLevelSolid || canDrawThingSolid))
        || canDrawSpriteSolid;

    const bool canDrawGrid =
        showGrid_
        && renderPipeline_ == RenderPipeline::Editor
        && width() > 0
        && height() > 0
        && ((gridProgram_ && gridQuadVao_)
            || (gridLineVao_ && gridLineVbo_));
    const render::GridDrawMode gridDrawMode = gridAlwaysBehind_
        ? render::GridDrawMode::AlwaysBehind
        : (gridVisibleThroughWalls_ ? render::GridDrawMode::VisibleThroughWalls
                                   : render::GridDrawMode::Occluded);
    const render::GridDrawPolicy gridDrawPolicy = render::gridDrawPolicy(gridDrawMode);

    const QColor effectiveSelectionPrimaryColor = cogEntityPickHighlightEnabled_ ? cogEntityPickPrimaryColor_ : selectionPrimaryColor_;
    const QColor effectiveSelectionMultiColor = cogEntityPickHighlightEnabled_ ? cogEntityPickPrimaryColor_.darker(130) : selectionMultiColor_;
    const QColor effectiveSelectionEdgeMultiColor = cogEntityPickHighlightEnabled_ ? cogEntityPickPrimaryColor_.darker(130) : selectionEdgeMultiColor_;
    const QColor effectiveSelectionActiveColor = cogEntityPickHighlightEnabled_ ? cogEntityPickActiveColor_ : selectionActiveColor_;
    const QColor effectiveSelectionVertexColor = cogEntityPickHighlightEnabled_ ? cogEntityPickPrimaryColor_ : selectionVertexColor_;
    const QColor effectiveSelectionVertexMultiColor = cogEntityPickHighlightEnabled_ ? cogEntityPickPrimaryColor_.darker(130) : selectionVertexMultiColor_;

    // Step 3: keep non-GL UI overlay drawing isolated so early exits and the normal end-of-frame path share it.
    const auto drawToastOverlay = [this]()
    {
        if (!showToastOverlays_)
        {
            return;
        }

        auto mapPlacement = [](const ToastPlacement placement) -> ui::viewport::ToastOverlayPlacement
        {
            switch (placement)
            {
            case ToastPlacement::Center:
                return ui::viewport::ToastOverlayPlacement::Center;
            case ToastPlacement::BottomCenter:
                return ui::viewport::ToastOverlayPlacement::BottomCenter;
            case ToastPlacement::TopLeft:
                return ui::viewport::ToastOverlayPlacement::TopLeft;
            }

            return ui::viewport::ToastOverlayPlacement::TopLeft;
        };

        auto mapGroup = [](const ToastGroup group) -> ui::viewport::ToastOverlayGroup
        {
            switch (group)
            {
            case ToastGroup::Projection:
                return ui::viewport::ToastOverlayGroup::Projection;
            case ToastGroup::FlySpeed:
                return ui::viewport::ToastOverlayGroup::FlySpeed;
            case ToastGroup::Error:
                return ui::viewport::ToastOverlayGroup::Error;
            case ToastGroup::General:
                return ui::viewport::ToastOverlayGroup::General;
            case ToastGroup::PlacementYaw:
                return ui::viewport::ToastOverlayGroup::PlacementYaw;
            case ToastGroup::Count:
                break;
            }

            return ui::viewport::ToastOverlayGroup::General;
        };

        std::array<ui::viewport::ToastOverlayToast, kToastGroupCount> toasts{};
        for (std::size_t i = 0; i < toastStates_.size(); ++i)
        {
            const ToastState& state = toastStates_[i];
            const ToastGroup group = static_cast<ToastGroup>(i);
            const ViewportBannerStyle& style = viewportBannerSettings_.styles[
                static_cast<std::size_t>(viewportBannerKindForToastGroup(group))];
            toasts[i] = ui::viewport::ToastOverlayToast{
                state.text,
                state.durationMs,
                state.shownAt,
                state.expires,
                mapPlacement(state.placement),
                mapGroup(group),
                style.textColor,
                style.backgroundColor,
                style.borderColor,
            };
        }

        ui::viewport::drawToastOverlay(*this, toasts);
    };

    if (!program_ || (!canDrawAnyLines && !canDrawSolid && !canDrawPoints && !canDrawGrid && placementPreviewLines_.empty()))
    {
        drawToastOverlay();
        return;
    }

    // Step 4: ensure render resources that depend on current asset state are ready before camera setup.
    if (canDrawSolid)
    {
        if (showTextures_)
        {
            ensureTextures();
        }
    }
    refreshAlphaAdjoinWireSupports();

    // Step 5: build view/projection matrices once and pass the same frame data to every draw pass.
    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
    // Only the four arrowhead line vertices per selected face depend on the
    // camera. Patch that compact VBO range on view changes; rebuilding the
    // selected geometry here would make orbiting scale with selection size.
    updateSurfaceNormalArrowheadsForView(eye, target);

    const QMatrix4x4 mvp = proj * view;

    const bool projectedSky =
        level_
        && showTextures_
        && (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
        && (renderPipeline_ == RenderPipeline::GamePreview);

    // Step 6: prepare optional preview transforms that the shader applies by pick id.
    struct FrameXformState
    {
        int enabled{};
        int pickId{};
        QMatrix4x4 matrix{};
        QVector3D colorMul{ 1.0f, 1.0f, 1.0f };
    };

    FrameXformState frameXform{};
    frameXform.matrix.setToIdentity();

    if (frameEditEnabled_ && level_)
    {
        const std::int32_t thingIndex = selectedThing_;
        if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level_->things.size())
        {
            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            if (std::isfinite(thing.x) && std::isfinite(thing.y) && std::isfinite(thing.z)
                && std::isfinite(thing.pch) && std::isfinite(thing.yaw) && std::isfinite(thing.rol))
            {
                if (selectedFrameValueIndex_ >= 0 && static_cast<std::size_t>(selectedFrameValueIndex_) < thing.values.size())
                {
                    const TplValue& fv = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
                    if (fv.type == ValueType::frame && sed::text::iequalsAscii(fv.name, "frame"))
                    {
                        const render::frame_ghost::FramePose* pose =
                            findFramePose(thingIndex, selectedFrameValueIndex_);
                        const double x = pose
                            ? pose->x
                            : std::numeric_limits<double>::quiet_NaN();
                        const double y = pose
                            ? pose->y
                            : std::numeric_limits<double>::quiet_NaN();
                        const double z = pose
                            ? pose->z
                            : std::numeric_limits<double>::quiet_NaN();
                        const double pch = pose
                            ? pose->pitch
                            : std::numeric_limits<double>::quiet_NaN();
                        const double yaw = pose
                            ? pose->yaw
                            : std::numeric_limits<double>::quiet_NaN();
                        const double rol = pose
                            ? pose->roll
                            : std::numeric_limits<double>::quiet_NaN();

                        const std::uint32_t pickIdU32 =
                            (static_cast<std::size_t>(thingIndex) < thingPickIds_.size())
                                ? thingPickIds_[static_cast<std::size_t>(thingIndex)]
                                : 0u;

                        if (pickIdU32 != 0
                            && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
                            && std::isfinite(pch) && std::isfinite(yaw) && std::isfinite(rol))
                        {
                            TVector thingPyr{};
                            thingPyr.pitch = thing.pch;
                            thingPyr.yaw = thing.yaw;
                            thingPyr.roll = thing.rol;

                            TVector framePyr{};
                            framePyr.pitch = pch;
                            framePyr.yaw = yaw;
                            framePyr.roll = rol;

                            const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, thingPyr);
                            const TMatrix43 frameXf = TMatrix43::Create(TVector{ x, y, z }, framePyr);

                            const QMatrix4x4 mThing = matrix43ToQt(thingXf);
                            bool invOk = false;
                            const QMatrix4x4 invThing = mThing.inverted(&invOk);
                            if (invOk)
                            {
                                frameXform.enabled = 1;
                                frameXform.pickId = static_cast<int>(pickIdU32);
                                frameXform.matrix = matrix43ToQt(frameXf) * invThing;
                            }
                        }
                    }
                }
            }
        }
    }

    // Step 7: bind the shared mesh/line/point shader and upload frame-wide uniforms.
    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_point_size", 1.0f);
    program_->setUniformValue("u_grid_minor_point_mode", false);
    program_->setUniformValue("u_grid_minor_point_diameter_px", 2.0f);
    program_->setUniformValue("u_grid_minor_point_raster_size_px", 3.0f);
    program_->setUniformValue("u_ndc_z_bias", 0.0f);
    program_->setUniformValue("u_outline_center", QVector3D(0.0f, 0.0f, 0.0f));
    program_->setUniformValue("u_outline_scale", 1.0f);
    program_->setUniformValue("u_outline_px", 0.0f);
    program_->setUniformValue("u_viewport_size", QVector2D(static_cast<float>(pixelW), static_cast<float>(pixelH)));
    program_->setUniformValue("u_outline_dir", QVector2D(1.0f, 0.0f));
    program_->setUniformValue("u_render_pipeline", projectedSky ? 1 : 0);
    program_->setUniformValue("u_camera_pos", eye);
    program_->setUniformValue("u_frame_xform_enabled", frameXform.enabled);
    program_->setUniformValue("u_frame_xform_id", frameXform.pickId);
    program_->setUniformValue("u_frame_xform_color_mul", frameXform.colorMul);
    program_->setUniformValue("u_frame_xform_matrix", frameXform.matrix);
    program_->setUniformValue("u_xform_enabled", transformPreviewActive_ ? 1 : 0);
    program_->setUniformValue("u_xform_count", transformPreviewActive_ ? transformPreviewIdCount_ : 0);
    if (transformPreviewActive_ && transformPreviewIdCount_ > 0)
    {
        program_->setUniformValueArray("u_xform_ids", transformPreviewIds_.data(), transformPreviewIdCount_);
        // Each entry independently uses this payload as either a complete
        // owner color multiplier or the exact source point of a shared corner.
        program_->setUniformValueArray(
            "u_xform_color_mul",
            transformPreviewPayloads_.data(),
            transformPreviewIdCount_);
    }
    program_->setUniformValue("u_xform_matrix", transformPreviewActive_ ? transformPreviewMatrix_ : QMatrix4x4());

    std::vector<render::lighting::RuntimeThingLight> runtimeThingLights{};
    if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
    {
        if ( level_ && thingLightRenderingEnabled_ )
        {
            std::vector<std::optional<QMatrix4x4>> sourceTransformOverrides{};
            std::vector<std::optional<std::int32_t>> sourceSectorOverrides{};
            if (transformPreviewActive_ && !transformPreviewVertexFilter_)
            {
                sourceTransformOverrides.resize(level_->things.size());
                sourceSectorOverrides.resize(level_->things.size());
                for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
                {
                    if (thingIndex >= thingPickIds_.size())
                    {
                        continue;
                    }
                    const int pickId = static_cast<int>(thingPickIds_[thingIndex]);
                    for (int i = 0; i < transformPreviewIdCount_; ++i)
                    {
                        if (transformPreviewIds_[static_cast<std::size_t>(i)] == pickId)
                        {
                            sourceTransformOverrides[thingIndex] = transformPreviewMatrix_;
                            if (const auto sectorIt = previewThingSectors_.find(
                                    static_cast<std::int32_t>(thingIndex));
                                sectorIt != previewThingSectors_.end())
                            {
                                sourceSectorOverrides[thingIndex] = sectorIt->second;
                            }
                            break;
                        }
                    }
                }
            }

            // Gouraud light is evaluated at original vertices before raster
            // clipping, so an emitter outside the camera frustum can still
            // affect a visible clipped primitive. Keep collection independent
            // of camera orientation, as the engines' visible-sector collector
            // does, and rely on the original bounded camera-light cap. Preview
            // sector overrides still suppress emitters while a Thing is outside
            // every valid sector.
            runtimeThingLights = render::lighting::collectRuntimeThingLights(
                *level_,
                templates_,
                mvp,
                eye,
                0,
                sourceTransformOverrides,
                sourceSectorOverrides,
                level_->kind == ProjectType::IJIM
                        && dynamicLightShading_ == DynamicLightShading::Phong
                    ? 1.25f
                    : 1.0f,
                showHiddenThings_
                    ? render::lighting::ThingLightOwnerPolicy::AuthoringPreview
                    : render::lighting::ThingLightOwnerPolicy::RuntimeParity,
                render::lighting::ThingLightCollectionPolicy::ViewStable);

            // Thing-light-only transforms do not mutate the document while the
            // mouse is held. Apply their authored semantics to the collected GPU
            // lights so the scene illumination follows the gizmo in real time.
            if (transformDrag_.active
                && transformDrag_.target == TransformTargetKind::ThingLight)
            {
                const bool moving = transformDrag_.handle == TransformHandle::MoveX
                    || transformDrag_.handle == TransformHandle::MoveY
                    || transformDrag_.handle == TransformHandle::MoveZ
                    || transformDrag_.handle == TransformHandle::MoveFree;
                const bool scaling = transformDrag_.handle == TransformHandle::ScaleX
                    || transformDrag_.handle == TransformHandle::ScaleY
                    || transformDrag_.handle == TransformHandle::ScaleZ
                    || transformDrag_.handle == TransformHandle::ScaleUniform;
                const auto sourceKind = transformDrag_.lightKind == render::LightGizmoKind::ThingHead
                    ? render::lighting::RuntimeThingLightSource::Headlight
                    : render::lighting::RuntimeThingLightSource::Regular;

                for (render::lighting::RuntimeThingLight& light : runtimeThingLights)
                {
                    const bool selectedOwner = light.sourceThing == transformDrag_.lightOwner
                        || highlightedSelection_.things.contains(light.sourceThing);
                    if (!selectedOwner || light.source != sourceKind)
                    {
                        continue;
                    }

                    if (moving && sourceKind == render::lighting::RuntimeThingLightSource::Headlight)
                    {
                        light.position += transformDrag_.translation;
                    }
                    else if (scaling)
                    {
                        const float scale = std::max(transformDrag_.scaleFactor, 0.001f);
                        light.minRadius *= scale;
                        light.maxRadius *= scale;
                        if (level_->kind != ProjectType::IJIM)
                        {
                            // Classic lights derive radius from scalar
                            // intensity, so their preview must scale both.
                            light.color *= scale;
                        }
                    }
                }
            }
        }
    }

    int uploadedDynamicLightCount = 0;
    if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
    {
        if (dynamicLightUniformBufferReady_ && dynamicLightUniformBuffer_ != 0)
        {
            QOpenGLContext* context = QOpenGLContext::currentContext();
            QOpenGLExtraFunctions* extra = context ? context->extraFunctions() : nullptr;
            bool uploaded = false;
            if (extra)
            {
                extra->initializeOpenGLFunctions();
                const render::lighting::DynamicLightGpuBlock lightBlock =
                    render::lighting::packDynamicLightGpuBlock(runtimeThingLights);
                GLint previousUniformBuffer = 0;
                extra->glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousUniformBuffer);
                extra->glBindBuffer(GL_UNIFORM_BUFFER, dynamicLightUniformBuffer_);
                void* mapped = extra->glMapBufferRange(
                    GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(sizeof(lightBlock)),
                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                if (mapped)
                {
                    std::memcpy(mapped, &lightBlock, sizeof(lightBlock));
                    uploaded = extra->glUnmapBuffer(GL_UNIFORM_BUFFER) == GL_TRUE;
                }
                if (uploaded)
                {
                    extra->glBindBufferBase(
                        GL_UNIFORM_BUFFER,
                        render::lighting::kDynamicLightUniformBlockBinding,
                        dynamicLightUniformBuffer_);
                }
                extra->glBindBuffer(
                    GL_UNIFORM_BUFFER,
                    static_cast<GLuint>(previousUniformBuffer));
            }

            if (uploaded)
            {
                uploadedDynamicLightCount = static_cast<int>(std::min(
                    runtimeThingLights.size(),
                    render::lighting::kIjimThingLightCap));
            }
            else
            {
                releaseDynamicLightUniformBuffer();
                if (!dynamicLightUniformBufferWarningEmitted_)
                {
                    dynamicLightUniformBufferWarningEmitted_ = true;
                    Q_EMIT warningMessage(QStringLiteral(
                        "Dynamic runtime lights are disabled: uniform-buffer update failed."));
                }
            }
        }
    }
    program_->setUniformValue(
        "u_dynamic_light_project",
        level_ ? static_cast<int>(level_->kind) : 0);
    program_->setUniformValue("u_dynamic_light_count", uploadedDynamicLightCount);
    program_->setUniformValue(
        "u_sector_point_lights_enabled",
        sectorPointLightRenderingEnabled_);
    program_->setUniformValue(
        "u_dynamic_lights_per_pixel",
        dynamicLightUniformBufferReady_
            && dynamicLightShading_ == DynamicLightShading::Phong);

    std::int32_t cameraSector = -1;
    if (level_ && level_->kind != ProjectType::IJIM)
    {
        // Fly navigation can cross many Sectors per second. Reuse the
        // viewport's BVH instead of testing every Sector surface on every
        // rendered frame merely to resolve the classic camera tint.
        cameraSector = findContainingSector(eye, classicCameraSector_);
        classicCameraSector_ = cameraSector;
    }
    const QVector3D classicSectorTint = render::classicCameraSectorTintScale(level_, cameraSector);
    program_->setUniformValue("u_classic_sector_tint_scale", classicSectorTint);
    program_->setUniformValue("u_apply_classic_sector_tint", false);
    program_->setUniformValue("u_apply_local_classic_sector_tint", false);

    constexpr int kSectorLightPreviewCap = 16;
    std::array<int, kSectorLightPreviewCap> sectorLightPreviewOwners{};
    std::array<QVector4D, kSectorLightPreviewCap> sectorLightPreviewPosMin{};
    std::array<QVector4D, kSectorLightPreviewCap> sectorLightPreviewColorMax{};
    int sectorLightPreviewCount = 0;
    if (level_
        && level_->kind == ProjectType::IJIM
        && transformDrag_.active
        && transformDrag_.target == TransformTargetKind::SectorLight)
    {
        std::vector<std::int32_t> owners{};
        owners.reserve(highlightedSelection_.sectors.size() + 1);
        owners.insert(
            owners.end(), highlightedSelection_.sectors.begin(), highlightedSelection_.sectors.end());
        owners.push_back(transformDrag_.lightOwner);
        std::sort(owners.begin(), owners.end());
        owners.erase(std::unique(owners.begin(), owners.end()), owners.end());

        const bool moving = transformDrag_.handle == TransformHandle::MoveX
            || transformDrag_.handle == TransformHandle::MoveY
            || transformDrag_.handle == TransformHandle::MoveZ
            || transformDrag_.handle == TransformHandle::MoveFree;
        const bool scaling = transformDrag_.handle == TransformHandle::ScaleX
            || transformDrag_.handle == TransformHandle::ScaleY
            || transformDrag_.handle == TransformHandle::ScaleZ
            || transformDrag_.handle == TransformHandle::ScaleUniform;

        for (const std::int32_t owner : owners)
        {
            if (sectorLightPreviewCount >= kSectorLightPreviewCap
                || owner < 0
                || static_cast<std::size_t>(owner) >= level_->sectors.size())
            {
                continue;
            }

            PointLight preview = level_->sectors[static_cast<std::size_t>(owner)].pointLight;
            if (moving)
            {
                preview.position.x += static_cast<double>(transformDrag_.translation.x());
                preview.position.y += static_cast<double>(transformDrag_.translation.y());
                preview.position.z += static_cast<double>(transformDrag_.translation.z());
            }
            else if (scaling)
            {
                const double scale = std::max(static_cast<double>(transformDrag_.scaleFactor), 0.001);
                // Authored zero is the original engine-derived-radius sentinel.
                // Keep it zero, exactly as the eventual commit does.
                if (preview.minRange != 0.0)
                {
                    preview.minRange = std::clamp(preview.minRange * scale, 0.001, 1.0e6);
                }
                if (preview.maxRange != 0.0)
                {
                    preview.maxRange = std::clamp(preview.maxRange * scale, 0.001, 1.0e6);
                }
            }

            const render::lighting::RuntimeSectorPointLight runtime =
                render::lighting::resolveRuntimeSectorPointLight(preview);
            const std::size_t slot = static_cast<std::size_t>(sectorLightPreviewCount++);
            sectorLightPreviewOwners[slot] = owner;
            sectorLightPreviewPosMin[slot] = QVector4D(
                runtime.position, runtime.minRadius);
            sectorLightPreviewColorMax[slot] = QVector4D(
                runtime.color, runtime.maxRadius);
        }
    }
    program_->setUniformValue("u_sector_light_preview_count", sectorLightPreviewCount);
    if (sectorLightPreviewCount > 0)
    {
        program_->setUniformValueArray(
            "u_sector_light_preview_owners",
            sectorLightPreviewOwners.data(),
            sectorLightPreviewCount);
        program_->setUniformValueArray(
            "u_sector_light_preview_pos_min",
            sectorLightPreviewPosMin.data(),
            sectorLightPreviewCount);
        program_->setUniformValueArray(
            "u_sector_light_preview_color_max",
            sectorLightPreviewColorMax.data(),
            sectorLightPreviewCount);
    }

    if (level_)
    {
        const Fog& fog = level_->header.fog;
        program_->setUniformValue(
            "u_fog_color",
            QVector4D(fog.color.r, fog.color.g, fog.color.b, fog.color.a));
        program_->setUniformValue("u_fog_start", ijimFogTableStart(fog));
        program_->setUniformValue("u_fog_end", ijimFogTableEnd(fog));
    }
    else
    {
        program_->setUniformValue("u_fog_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        program_->setUniformValue("u_fog_start", 0.0f);
        program_->setUniformValue("u_fog_end", 1.0f);
    }
    program_->setUniformValue("u_fog_enabled", false);

    // Step 8: configure projected sky uniforms only for textured game-preview perspective renders.
    if (level_ && projectedSky)
    {
        const float canvasW = static_cast<float>(camera_.canvasWidth());
        const float canvasH = static_cast<float>(camera_.canvasHeight());
        const float canvasX = (canvasW + 1.0f) * 0.5f;
        const float canvasY = (canvasH + 1.0f) * 0.5f;

        const LevelHeader& header = level_->header;
        program_->setUniformValue("u_sky_ceiling_height", static_cast<float>(header.ceilingSky.height));
        program_->setUniformValue(
            "u_sky_ceiling_offset",
            QVector2D(static_cast<float>(header.ceilingSky.offset.x), static_cast<float>(header.ceilingSky.offset.y)));
        program_->setUniformValue("u_sky_canvas_half", QVector2D(canvasX, canvasY));

        double horizonSkyPPR = 1.0 / 360.0;
        double horizonSkyDistance = 0.0;
        if (std::abs(header.horizonSky.pixelsPerRev) > 1e-9)
        {
            horizonSkyDistance = header.horizonSky.distance / header.horizonSky.pixelsPerRev;
        }
        if (level_->kind == ProjectType::IJIM)
        {
            horizonSkyPPR = 1.0 / 120.0;
            horizonSkyDistance = header.horizonSky.distance / 256.0;
        }

        const double horizonSkyScale =
            (std::abs(camera_.focalLength()) > 1e-6f) ? (horizonSkyDistance / static_cast<double>(camera_.focalLength())) : 0.0;

        // Keep the horizon at the far plane so it never occludes world/thing geometry.
        // We draw the sky with depth-func LEQUAL so z=1.0 still passes against the cleared depth buffer.
        const float horizonNdcZ = 1.0f;

        const QVector3D pyrDeg = camera_.pyrDeg();
        const double horizonYaw   = -(static_cast<double>(pyrDeg.y()) * horizonSkyPPR);
        const double horizonPitch = -(static_cast<double>(pyrDeg.x()) * horizonSkyPPR);

        const double rollRad = deg2Rad(static_cast<double>(pyrDeg.z()));
        const double horizonRollSin = std::sin(rollRad);
        const double horizonRollCos = std::cos(rollRad);

        program_->setUniformValue("u_sky_horizon_scale", static_cast<float>(horizonSkyScale));
        program_->setUniformValue("u_sky_horizon_ndc_z", horizonNdcZ);
        program_->setUniformValue(
            "u_sky_horizon_offset",
            QVector2D(static_cast<float>(header.horizonSky.offset.x), static_cast<float>(header.horizonSky.offset.y)));
        program_->setUniformValue(
            "u_sky_horizon_yaw_pitch",
            QVector2D(static_cast<float>(horizonYaw), static_cast<float>(horizonPitch)));
        program_->setUniformValue(
            "u_sky_horizon_roll_sin_cos",
            QVector2D(static_cast<float>(horizonRollSin), static_cast<float>(horizonRollCos)));
    }
    else
    {
        program_->setUniformValue("u_sky_ceiling_height", 0.0f);
        program_->setUniformValue("u_sky_ceiling_offset", QVector2D(0.0f, 0.0f));
        program_->setUniformValue("u_sky_canvas_half", QVector2D(1.0f, 1.0f));
        program_->setUniformValue("u_sky_horizon_scale", 0.0f);
        program_->setUniformValue("u_sky_horizon_ndc_z", 1.0f);
        program_->setUniformValue("u_sky_horizon_offset", QVector2D(0.0f, 0.0f));
        program_->setUniformValue("u_sky_horizon_yaw_pitch", QVector2D(0.0f, 0.0f));
        program_->setUniformValue("u_sky_horizon_roll_sin_cos", QVector2D(0.0f, 1.0f));
    }

    auto drawGridPass = [&](const bool depthTestScene)
    {
        if (!canDrawGrid)
        {
            return;
        }

        const int widgetW = width();
        const int widgetH = height();

        // Step 1: resolve which world plane the grid should live on for this frame.
        struct GridAxes
        {
            QVector3D axisA{ 1.0f, 0.0f, 0.0f };
            QVector3D axisB{ 0.0f, 1.0f, 0.0f };
        };

        struct GridParams
        {
            QVector3D axisA{ 1.0f, 0.0f, 0.0f };
            QVector3D axisB{ 0.0f, 1.0f, 0.0f };
            QVector3D planePoint{};
            QVector3D planeNormal{ 0.0f, 0.0f, 1.0f };
            QVector3D viewCenter{};
            QVector3D coordOrigin{};
            QVector2D minorCoordPhase{};
            QVector2D majorCoordPhase{};
            QVector2D majorLod1CoordPhase{};
            QVector2D majorLod2CoordPhase{};
            QVector2D majorLod3CoordPhase{};
            float unitsPerPx{};
            float halfVisible{ 1.0f };
            double majorSpacing{ 1.0 };
            double minorSpacing{ 0.1 };
            std::int64_t majorEvery{ 10 };
            bool valid{};
        };

        auto resolveGridAxes = [&]() -> GridAxes
        {
            GridAxes axes{};
            if (gridAlignToView_)
            {
                QVector3D normal{};
                if (!render::orthonormalizeGridAxes(
                        gridAlignAxisA_, gridAlignAxisB_, axes.axisA, axes.axisB, normal))
                {
                    // A malformed recovery payload must not poison the grid
                    // frame (NaNs here otherwise spread through every
                    // projected line and make the whole viewport flicker).
                    axes.axisA = QVector3D(1.0f, 0.0f, 0.0f);
                    axes.axisB = QVector3D(0.0f, 1.0f, 0.0f);
                }
            }
            else if (gridRotateWithView_)
            {
                bool invViewOk = false;
                const QMatrix4x4 invView = view.inverted(&invViewOk);
                QVector3D cameraRight{};
                QVector3D cameraUp{};
                if (invViewOk)
                {
                    const QVector4D r4 = invView.column(0);
                    const QVector4D u4 = invView.column(1);
                    cameraRight = QVector3D(r4.x(), r4.y(), r4.z());
                    cameraUp = QVector3D(u4.x(), u4.y(), u4.z());
                }
                const render::ViewAlignedGridFrame viewFrame =
                    render::makeViewAlignedGridFrame(
                        cameraRight,
                        cameraUp,
                        target - eye);
                axes.axisA = viewFrame.axisA;
                axes.axisB = viewFrame.axisB;
            }
            else
            {
                switch (gridPlane_)
                {
                    case GridPlane::XY:
                        axes.axisA = QVector3D(1.0f, 0.0f, 0.0f);
                        axes.axisB = QVector3D(0.0f, 1.0f, 0.0f);
                        break;
                    case GridPlane::XZ:
                        axes.axisA = QVector3D(1.0f, 0.0f, 0.0f);
                        axes.axisB = QVector3D(0.0f, 0.0f, 1.0f);
                        break;
                    case GridPlane::YZ:
                        axes.axisA = QVector3D(0.0f, 1.0f, 0.0f);
                        axes.axisB = QVector3D(0.0f, 0.0f, 1.0f);
                        break;
                }
            }

            return axes;
        };

        const bool gridUsesAuthoredProjection =
            gridAlwaysBehind_ || gridVisibleThroughWalls_;
        const QMatrix4x4 gridMvp = (gridUsesAuthoredProjection
                ? camera_.projectionMatrix()
                : proj) * view;
        bool invOk = false;
        const QMatrix4x4 invMvp = gridMvp.inverted(&invOk);
        const QVector3D defaultGridCenter = gridFollowCamera_ ? target : QVector3D(0.0f, 0.0f, 0.0f);
        const QVector3D gridCenter = gridCenterOverride_.value_or(defaultGridCenter);
        const GridAxes gridAxes = resolveGridAxes();
        const render::ViewAlignedGridDisplayPlane viewAlignedDisplay =
            render::resolveViewAlignedGridDisplayPlane(
                gridCenter,
                eye,
                target - eye,
                nearPlane,
                gridRotateWithView_ && gridKeepViewAlignedVisible_);

        // Step 2: estimate world-units-per-pixel so adaptive spacing and fade radius remain stable while zooming.
        GridParams grid{};
        grid.axisA = gridAxes.axisA;
        grid.axisB = gridAxes.axisB;
        grid.planePoint = (gridCenterOverride_.has_value() || gridAlignToView_ || gridRotateWithView_)
            ? (gridRotateWithView_ ? viewAlignedDisplay.planePoint : gridCenter)
            : (grid.axisA * QVector3D::dotProduct(gridCenter, grid.axisA) + grid.axisB * QVector3D::dotProduct(gridCenter, grid.axisB));

        grid.planeNormal = QVector3D::crossProduct(grid.axisA, grid.axisB);
        grid.valid = normalizeInPlace(grid.planeNormal);

        const bool isOrtho = camera_.projectionType()
            == app::ViewportCamera::ProjectionType::Orthographic;
        if (grid.valid && invOk)
        {
            const QPoint centerPx(widgetW / 2, widgetH / 2);
            const QPoint centerPxRight(centerPx.x() + 1, centerPx.y());
            const auto p0 = intersectWidgetRayWithPlane(
                centerPx, invMvp, eye, isOrtho, widgetW, widgetH, grid.planePoint, grid.planeNormal);
            const auto p1 = intersectWidgetRayWithPlane(
                centerPxRight, invMvp, eye, isOrtho, widgetW, widgetH, grid.planePoint, grid.planeNormal);
            if (p0 && p1)
            {
                grid.unitsPerPx = (*p1 - *p0).length();
            }
        }

        const float projectionUnitsPerPixel =
            (!std::isfinite(grid.unitsPerPx) || grid.unitsPerPx <= 0.0f)
                && grid.valid && invOk
                ? worldUnitsPerPixelAt(
                    grid.planePoint, gridMvp, invMvp, widgetW, widgetH)
                : 0.0f;
        grid.unitsPerPx = render::resolveGridUnitsPerPixel(
            grid.unitsPerPx,
            projectionUnitsPerPixel,
            isOrtho,
            camera_.orthoScale(),
            widgetH);

        const render::EffectiveGridSpacing spacing = render::effectiveGridSpacing(
            gridMajorSpacing_, gridMinorSpacing_, gridAdaptiveSpacing_, grid.unitsPerPx);
        grid.majorSpacing = spacing.major;
        grid.minorSpacing = spacing.minor;
        grid.majorEvery = spacing.majorEvery;

        const QVector3D centerWorld = gridCenterOverride_.value_or(defaultGridCenter);
        grid.viewCenter = (gridCenterOverride_.has_value() || gridAlignToView_ || gridRotateWithView_)
            ? (gridRotateWithView_ ? viewAlignedDisplay.planePoint : centerWorld)
            : (grid.axisA * QVector3D::dotProduct(centerWorld, grid.axisA) + grid.axisB * QVector3D::dotProduct(centerWorld, grid.axisB));

        if (grid.valid && gridFollowCamera_ && !gridCenterOverride_.has_value() && !gridAlignToView_ && !gridRotateWithView_)
        {
            const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
            const QPoint centerPx(widgetW / 2, widgetH / 2);
            if (const auto hitOpt = intersectWidgetRayWithPlane(
                    centerPx, invMvp, eye, isOrtho, widgetW, widgetH, grid.viewCenter, grid.planeNormal))
            {
                grid.viewCenter = *hitOpt;
            }
        }

        grid.coordOrigin = (gridCenterOverride_.has_value() || gridAlignToView_ || gridRotateWithView_)
            ? (gridRotateWithView_ ? viewAlignedDisplay.coordOrigin : grid.viewCenter)
            : QVector3D(0.0f, 0.0f, 0.0f);

        {
            const QVector3D centerRel = grid.viewCenter - grid.coordOrigin;
            const double coordA = static_cast<double>(QVector3D::dotProduct(centerRel, grid.axisA));
            const double coordB = static_cast<double>(QVector3D::dotProduct(centerRel, grid.axisB));
            grid.minorCoordPhase = QVector2D(
                render::stableGridCoordPhase(coordA, grid.minorSpacing),
                render::stableGridCoordPhase(coordB, grid.minorSpacing));
            grid.majorCoordPhase = QVector2D(
                render::stableGridCoordPhase(coordA, grid.majorSpacing),
                render::stableGridCoordPhase(coordB, grid.majorSpacing));
            const double majorLod1Spacing = render::gridHierarchySpacing(grid.majorSpacing, 1);
            const double majorLod2Spacing = render::gridHierarchySpacing(grid.majorSpacing, 2);
            const double majorLod3Spacing = render::gridHierarchySpacing(grid.majorSpacing, 3);
            grid.majorLod1CoordPhase = QVector2D(
                render::stableGridCoordPhase(coordA, majorLod1Spacing),
                render::stableGridCoordPhase(coordB, majorLod1Spacing));
            grid.majorLod2CoordPhase = QVector2D(
                render::stableGridCoordPhase(coordA, majorLod2Spacing),
                render::stableGridCoordPhase(coordB, majorLod2Spacing));
            grid.majorLod3CoordPhase = QVector2D(
                render::stableGridCoordPhase(coordA, majorLod3Spacing),
                render::stableGridCoordPhase(coordB, majorLod3Spacing));
        }

        grid.halfVisible = grid.unitsPerPx * 0.6f * static_cast<float>(std::max(widgetW, widgetH));
        if (grid.valid && invOk)
        {
            const std::array<QPoint, 4> corners = {
                QPoint(0, 0),
                QPoint(widgetW, 0),
                QPoint(0, widgetH),
                QPoint(widgetW, widgetH),
            };

            double maxOff = 0.0;
            int hitCount = 0;
            for (const QPoint& c : corners)
            {
                if (const auto hitOpt = intersectWidgetRayWithPlane(
                        c, invMvp, eye, isOrtho, widgetW, widgetH, grid.viewCenter, grid.planeNormal))
                {
                    const QVector3D d = *hitOpt - grid.viewCenter;
                    const double a = std::abs(static_cast<double>(QVector3D::dotProduct(d, grid.axisA)));
                    const double b = std::abs(static_cast<double>(QVector3D::dotProduct(d, grid.axisB)));
                    if (std::isfinite(a))
                    {
                        maxOff = std::max(maxOff, a);
                    }
                    if (std::isfinite(b))
                    {
                        maxOff = std::max(maxOff, b);
                    }
                    ++hitCount;
                }
            }

            if (hitCount >= 2 && std::isfinite(maxOff) && maxOff > 0.0)
            {
                grid.halfVisible = static_cast<float>(maxOff);
            }
        }

        // At a very close perspective zoom the four corner rays can be nearly
        // parallel to a view-aligned plane, yielding a tiny/unstable footprint.
        // Keep at least eight major cells available so the analytic grid does
        // not blink out while the camera crosses that singular configuration.
        grid.halfVisible = std::max(
            grid.halfVisible,
            render::gridMinimumVisibleHalfExtent(grid.majorSpacing));

        const bool useShaderGrid = grid.valid && gridProgram_ && gridQuadVao_;

        // Step 3: prefer the shader grid, falling back to cached line geometry on unsupported paths.
        if (useShaderGrid)
        {
            f->glDisable(GL_CULL_FACE);
            f->glDisable(GL_POLYGON_OFFSET_FILL);
            if (depthTestScene)
            {
                f->glEnable(GL_DEPTH_TEST);
                // Coplanar fragments are a supported grid use case.  LEQUAL
                // plus the mode-specific tiny pull makes their outcome
                // independent of the previous pass' depth function and float
                // reconstruction rounding.
                f->glDepthFunc(GL_LEQUAL);
            }
            else
            {
                f->glDisable(GL_DEPTH_TEST);
            }
            f->glDepthMask(GL_FALSE);
            f->glEnable(GL_BLEND);
            f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const float opacity = std::clamp(gridOpacity_, 0.0f, 1.0f);
            QColor minorColor = gridColor_;
            minorColor.setAlphaF(opacity);
            QColor majorColor = gridMajorColor_;
            majorColor.setAlphaF(opacity);
            QColor axisAColor = render::gridAxisColorForDirection(grid.axisA, gridAxisXColor_, gridAxisYColor_, gridAxisZColor_);
            axisAColor.setAlphaF(opacity);
            QColor axisBColor = render::gridAxisColorForDirection(grid.axisB, gridAxisXColor_, gridAxisYColor_, gridAxisZColor_);
            axisBColor.setAlphaF(opacity);

            const float fadeRadius = std::max(grid.halfVisible * 1.15f, static_cast<float>(grid.majorSpacing) * 20.0f);
            const float fadeSoftness = std::max(grid.halfVisible * 0.85f, static_cast<float>(grid.majorSpacing) * 14.0f);
            const render::GridClipFadePolicy clipFadePolicy = render::gridClipFadePolicy();
            const render::GridFootprintFadePolicy footprintFadePolicy = render::gridFootprintFadePolicy();

            gridProgram_->bind();
            gridProgram_->setUniformValue("u_inv_mvp", invMvp);
            gridProgram_->setUniformValue("u_mvp", gridMvp);
            gridProgram_->setUniformValue("u_camera_pos", eye);
            gridProgram_->setUniformValue("u_plane_point", grid.planePoint);
            gridProgram_->setUniformValue("u_plane_normal", grid.planeNormal);
            gridProgram_->setUniformValue("u_axis_a", grid.axisA);
            gridProgram_->setUniformValue("u_axis_b", grid.axisB);
            // These direction projections are shared by all ten periodic-line
            // evaluations in the fragment shader. Keep the per-fragment work
            // to a scaled add and homogeneous cross product per candidate.
            gridProgram_->setUniformValue(
                "u_axis_a_clip_direction",
                gridMvp * QVector4D(grid.axisA, 0.0f));
            gridProgram_->setUniformValue(
                "u_axis_b_clip_direction",
                gridMvp * QVector4D(grid.axisB, 0.0f));
            gridProgram_->setUniformValue("u_coord_origin", grid.coordOrigin);
            gridProgram_->setUniformValue("u_view_center", grid.viewCenter);
            gridProgram_->setUniformValue("u_minor_coord_phase", grid.minorCoordPhase);
            gridProgram_->setUniformValue("u_major_coord_phase", grid.majorCoordPhase);
            gridProgram_->setUniformValue("u_major_lod1_coord_phase", grid.majorLod1CoordPhase);
            gridProgram_->setUniformValue("u_major_lod2_coord_phase", grid.majorLod2CoordPhase);
            gridProgram_->setUniformValue("u_major_lod3_coord_phase", grid.majorLod3CoordPhase);
            gridProgram_->setUniformValue(
                "u_is_ortho",
                camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic ? 1 : 0);
            gridProgram_->setUniformValue("u_minor_spacing", static_cast<float>(grid.minorSpacing));
            gridProgram_->setUniformValue("u_major_spacing", static_cast<float>(grid.majorSpacing));
            // Per-fragment derivatives select the hierarchy locally across a
            // perspective plane; a single centre-pixel visibility value is
            // incorrect when the near and horizon footprints differ greatly.
            gridProgram_->setUniformValue("u_minor_visibility", 1.0f);
            gridProgram_->setUniformValue("u_major_visibility", 1.0f);
            gridProgram_->setUniformValue("u_minor_points_enabled", gridMinorDots_ ? 1 : 0);
            gridProgram_->setUniformValue(
                "u_minor_dot_diameter_px",
                render::gridPhysicalMinorDotSizePx(gridMinorDotSizePx_, dpr));
            gridProgram_->setUniformValue("u_fade_radius", fadeRadius);
            gridProgram_->setUniformValue("u_fade_softness", fadeSoftness);
            gridProgram_->setUniformValue(
                "u_clip_fade_ratios",
                QVector2D(clipFadePolicy.startRatio, clipFadePolicy.endRatio));
            gridProgram_->setUniformValue(
                "u_minor_footprint_fade_px",
                QVector2D(
                    footprintFadePolicy.minorHiddenPixelsPerCell,
                    footprintFadePolicy.minorFullyVisiblePixelsPerCell));
            gridProgram_->setUniformValue(
                "u_major_footprint_fade_px",
                QVector2D(
                    footprintFadePolicy.majorHiddenPixelsPerCell,
                    footprintFadePolicy.majorFullyVisiblePixelsPerCell));
            const float physicalGridLineWidthPx =
                render::gridPhysicalLineWidthPx(gridLineWidthPx_, dpr);
            const float physicalGridAxisLineWidthPx =
                render::gridPhysicalAxisLineWidthPx(gridLineWidthPx_, dpr);
            gridProgram_->setUniformValue("u_line_width_px", physicalGridLineWidthPx);
            const render::GridProjectedAxisLine axisAScreenLine =
                render::projectedGridAxisLine(gridMvp, grid.coordOrigin, grid.axisA);
            const render::GridProjectedAxisLine axisBScreenLine =
                render::projectedGridAxisLine(gridMvp, grid.coordOrigin, grid.axisB);
            gridProgram_->setUniformValue(
                "u_axis_line_width_px",
                physicalGridAxisLineWidthPx);
            gridProgram_->setUniformValue(
                "u_viewport_size",
                QVector2D(static_cast<float>(pixelW), static_cast<float>(pixelH)));
            gridProgram_->setUniformValue(
                "u_axis_a_screen_line",
                axisAScreenLine.valid ? axisAScreenLine.equation : QVector3D());
            gridProgram_->setUniformValue(
                "u_axis_b_screen_line",
                axisBScreenLine.valid ? axisBScreenLine.equation : QVector3D());
            gridProgram_->setUniformValue("u_ndc_z_bias", gridDrawPolicy.coplanarNdcBias);
            gridProgram_->setUniformValue(
                "u_minor_color",
                QVector4D(minorColor.redF(), minorColor.greenF(), minorColor.blueF(), minorColor.alphaF()));
            gridProgram_->setUniformValue(
                "u_major_color",
                QVector4D(majorColor.redF(), majorColor.greenF(), majorColor.blueF(), majorColor.alphaF()));
            gridProgram_->setUniformValue(
                "u_axis_a_color",
                QVector4D(axisAColor.redF(), axisAColor.greenF(), axisAColor.blueF(), axisAColor.alphaF()));
            gridProgram_->setUniformValue(
                "u_axis_b_color",
                QVector4D(axisBColor.redF(), axisBColor.greenF(), axisBColor.blueF(), axisBColor.alphaF()));

            gridQuadVao_->bind();
            f->glDrawArrays(GL_TRIANGLES, 0, 3);
            gridQuadVao_->release();
            gridProgram_->release();

            program_->bind();
            f->glEnable(GL_CULL_FACE);
            f->glCullFace(GL_BACK);
            f->glEnable(GL_POLYGON_OFFSET_FILL);
            f->glDisable(GL_BLEND);
            f->glDepthMask(GL_TRUE);
            f->glEnable(GL_DEPTH_TEST);
            f->glDepthFunc(GL_LESS);
        }
        else if (gridLineVao_ && gridLineVbo_)
        {
            const bool needRebuild =
                !gridCacheValid_
                || gridCacheViewSerial_ != viewSerial_
                || gridCacheWidgetW_ != widgetW
                || gridCacheWidgetH_ != widgetH;

            if (needRebuild)
            {
                gridLineVertices_.clear();
                gridLineVertexCount_ = 0;
                gridLineMinorPointVertexCount_ = 0;
                gridLineMinorVertexCount_ = 0;
                gridLineMajorVertexCount_ = 0;
                gridLineAxisAVertexCount_ = 0;
                gridLineAxisBVertexCount_ = 0;

                if (grid.valid)
                {
                    render::GridLineBuildParams params{};
                    params.axisA = grid.axisA;
                    params.axisB = grid.axisB;
                    params.coordOrigin = grid.coordOrigin;
                    params.viewCenter = grid.viewCenter;
                    params.halfVisible = grid.halfVisible;
                    params.majorSpacing = grid.majorSpacing;
                    params.minorSpacing = grid.minorSpacing;
                    params.majorEvery = grid.majorEvery;
                    params.minorDots = gridMinorDots_;

                    const render::GridLineBuild lineBuild = render::buildGridLineVertices(params);
                    gridLineVertices_.reserve(lineBuild.vertices.size());
                    for (const render::GridLineVertex& src : lineBuild.vertices)
                    {
                        LineVertex dst{};
                        dst.x = src.x;
                        dst.y = src.y;
                        dst.z = src.z;
                        dst.pickId = src.pickId;
                        gridLineVertices_.push_back(dst);
                    }
                    gridLineMinorPointVertexCount_ = lineBuild.minorPointVertexCount;
                    gridLineMinorVertexCount_ = lineBuild.minorVertexCount;
                    gridLineMajorVertexCount_ = lineBuild.majorVertexCount;
                    gridLineAxisAVertexCount_ = lineBuild.axisAVertexCount;
                    gridLineAxisBVertexCount_ = lineBuild.axisBVertexCount;
                    gridLineVertexCount_ = static_cast<int>(gridLineVertices_.size());
                }

                gridLineVao_->bind();
                gridLineVbo_->bind();
                const std::size_t gridByteCount = gridLineVertices_.size() * sizeof(LineVertex);
                bool uploadedGridBuffer = false;
                if (gridByteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    gridLineVertices_.clear();
                    gridLineVertexCount_ = 0;
                    gridLineMinorPointVertexCount_ = 0;
                    gridLineMinorVertexCount_ = 0;
                    gridLineMajorVertexCount_ = 0;
                    gridLineAxisAVertexCount_ = 0;
                    gridLineAxisBVertexCount_ = 0;
                    gridLineVbo_->release();
                    gridLineVao_->release();
                }
                else if (gridByteCount == 0)
                {
                    gridLineVbo_->release();
                    gridLineVao_->release();
                }
                else
                {
                    const int gridBytes = static_cast<int>(gridByteCount);
                    if (gridLineVboAllocatedBytes_ < gridBytes)
                    {
                        const int half = gridLineVboAllocatedBytes_ / 2;
                        const int grown = gridLineVboAllocatedBytes_ + std::max(half, 4096);
                        const int newCap =
                            (gridLineVboAllocatedBytes_ <= 0)
                                ? gridBytes
                                : std::max(gridBytes, grown);
                        gridLineVbo_->allocate(nullptr, newCap);
                        gridLineVboAllocatedBytes_ = newCap;
                    }
                    gridLineVbo_->write(0, gridLineVertices_.data(), gridBytes);
                    uploadedGridBuffer = true;
                }
                if (uploadedGridBuffer)
                {
                    gridLineVbo_->release();
                    gridLineVao_->release();
                }

                gridCacheViewSerial_ = viewSerial_;
                gridCacheWidgetW_ = widgetW;
                gridCacheWidgetH_ = widgetH;
                gridCacheValid_ = true;
            }

            if (gridLineVertexCount_ > 0)
            {
                const bool gridLineSmoothWasEnabled =
                    f->glIsEnabled(GL_LINE_SMOOTH) == GL_TRUE;
                const bool gridMultisampleWasEnabled =
                    f->glIsEnabled(GL_MULTISAMPLE) == GL_TRUE;
                GLint gridFramebufferSamples = 0;
                f->glGetIntegerv(GL_SAMPLES, &gridFramebufferSamples);
                GLint previousGridLineSmoothHint = GL_DONT_CARE;
                f->glGetIntegerv(GL_LINE_SMOOTH_HINT, &previousGridLineSmoothHint);
                GLfloat previousGridLineWidth = 1.0f;
                f->glGetFloatv(GL_LINE_WIDTH, &previousGridLineWidth);
                if (!std::isfinite(previousGridLineWidth) || previousGridLineWidth <= 0.0f)
                {
                    previousGridLineWidth = 1.0f;
                }

                f->glDisable(GL_CULL_FACE);
                f->glDisable(GL_POLYGON_OFFSET_FILL);
                if (depthTestScene)
                {
                    f->glEnable(GL_DEPTH_TEST);
                    f->glDepthFunc(GL_LEQUAL);
                }
                else
                {
                    f->glDisable(GL_DEPTH_TEST);
                }
                f->glDepthMask(GL_FALSE);

                f->glEnable(GL_BLEND);
                f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                // Prefer actual framebuffer multisampling. If Qt/the driver
                // supplied a zero-sample surface, do not knowingly fall back
                // to hard one-pixel GL lines: line smoothing is imperfect and
                // implementation-defined, but is the only AA facility left on
                // this already-rare shader-compilation fallback path.
                if (gridFramebufferSamples > 0)
                {
                    f->glEnable(GL_MULTISAMPLE);
                    f->glDisable(GL_LINE_SMOOTH);
                }
                else
                {
                    f->glDisable(GL_MULTISAMPLE);
                    f->glEnable(GL_LINE_SMOOTH);
                    f->glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
                }

                program_->setUniformValue("u_lighting_mode", 0);
                program_->setUniformValue("u_geo_min", 0);
                program_->setUniformValue("u_textured", 0);
                program_->setUniformValue("u_alpha_test", 0);
                program_->setUniformValue("u_flat_shade", false);
                program_->setUniformValue("u_ndc_z_bias", gridDrawPolicy.coplanarNdcBias);
                program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
                program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));
                program_->setUniformValue("u_grid_minor_point_mode", false);

                const float opacity = std::clamp(gridOpacity_, 0.0f, 1.0f);
                QColor minorColor = gridColor_;
                minorColor.setAlphaF(opacity);
                QColor majorColor = gridMajorColor_;
                majorColor.setAlphaF(opacity);

                gridLineVao_->bind();
                f->glLineWidth(render::gridPhysicalLineWidthPx(gridLineWidthPx_, dpr));

                if ( gridLineMinorPointVertexCount_ > 0 )
                {
                    const bool pointSizeModeWasEnabled =
                        f->glIsEnabled(GL_PROGRAM_POINT_SIZE) == GL_TRUE;
                    const GLint pointSizeLocation =
                        program_->uniformLocation("u_point_size");
                    GLfloat previousPointSize = 1.0f;
                    if ( pointSizeLocation >= 0 )
                    {
                        f->glGetUniformfv(
                            program_->programId(),
                            pointSizeLocation,
                            &previousPointSize);
                    }
                    if ( !std::isfinite(previousPointSize)
                        || previousPointSize <= 0.0f )
                    {
                        previousPointSize = 1.0f;
                    }

                    const float physicalDotSizePx =
                        render::gridPhysicalMinorDotSizePx(gridMinorDotSizePx_, dpr);
                    const float physicalPointRasterSizePx = physicalDotSizePx + 1.0f;
                    f->glEnable(GL_PROGRAM_POINT_SIZE);
                    program_->setUniformValue("u_grid_minor_point_mode", true);
                    program_->setUniformValue("u_grid_minor_point_diameter_px", physicalDotSizePx);
                    program_->setUniformValue(
                        "u_grid_minor_point_raster_size_px",
                        physicalPointRasterSizePx);
                    program_->setUniformValue("u_point_size", physicalPointRasterSizePx);
                    program_->setUniformValue(
                        "u_color",
                        QVector4D(minorColor.redF(), minorColor.greenF(), minorColor.blueF(), minorColor.alphaF()));
                    f->glDrawArrays(GL_POINTS, 0, gridLineMinorPointVertexCount_);
                    program_->setUniformValue("u_grid_minor_point_mode", false);
                    program_->setUniformValue("u_point_size", previousPointSize);
                    program_->setUniformValue("u_grid_minor_point_diameter_px", 2.0f);
                    program_->setUniformValue("u_grid_minor_point_raster_size_px", 3.0f);
                    if ( pointSizeModeWasEnabled )
                    {
                        f->glEnable(GL_PROGRAM_POINT_SIZE);
                    }
                    else
                    {
                        f->glDisable(GL_PROGRAM_POINT_SIZE);
                    }
                }

                if (gridLineMinorVertexCount_ > 0)
                {
                    program_->setUniformValue(
                        "u_color",
                        QVector4D(minorColor.redF(), minorColor.greenF(), minorColor.blueF(), minorColor.alphaF()));
                    f->glDrawArrays(GL_LINES, 0, gridLineMinorVertexCount_);
                }

                if (gridLineMajorVertexCount_ > 0)
                {
                    program_->setUniformValue(
                        "u_color",
                        QVector4D(majorColor.redF(), majorColor.greenF(), majorColor.blueF(), majorColor.alphaF()));
                    f->glDrawArrays(
                        GL_LINES,
                        gridLineMinorPointVertexCount_ + gridLineMinorVertexCount_,
                        gridLineMajorVertexCount_);
                }

                const int axisAFirst = gridLineMinorPointVertexCount_
                    + gridLineMinorVertexCount_ + gridLineMajorVertexCount_;
                const int axisBFirst = axisAFirst + gridLineAxisAVertexCount_;
                f->glLineWidth(render::gridPhysicalAxisLineWidthPx(gridLineWidthPx_, dpr));

                if (gridLineAxisAVertexCount_ > 0)
                {
                    QColor c = render::gridAxisColorForDirection(grid.axisA, gridAxisXColor_, gridAxisYColor_, gridAxisZColor_);
                    c.setAlphaF(opacity);
                    program_->setUniformValue("u_color", QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
                    f->glDrawArrays(GL_LINES, axisAFirst, gridLineAxisAVertexCount_);
                }

                if (gridLineAxisBVertexCount_ > 0)
                {
                    QColor c = render::gridAxisColorForDirection(grid.axisB, gridAxisXColor_, gridAxisYColor_, gridAxisZColor_);
                    c.setAlphaF(opacity);
                    program_->setUniformValue("u_color", QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
                    f->glDrawArrays(GL_LINES, axisBFirst, gridLineAxisBVertexCount_);
                }

                gridLineVao_->release();
                f->glLineWidth(previousGridLineWidth);
                if (gridLineSmoothWasEnabled)
                {
                    f->glEnable(GL_LINE_SMOOTH);
                }
                else
                {
                    f->glDisable(GL_LINE_SMOOTH);
                }
                f->glHint(
                    GL_LINE_SMOOTH_HINT,
                    static_cast<GLenum>(previousGridLineSmoothHint));
                if (gridMultisampleWasEnabled)
                {
                    f->glEnable(GL_MULTISAMPLE);
                }
                else
                {
                    f->glDisable(GL_MULTISAMPLE);
                }

                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
                f->glEnable(GL_POLYGON_OFFSET_FILL);
                f->glDisable(GL_BLEND);
                f->glDepthMask(GL_TRUE);
                f->glEnable(GL_DEPTH_TEST);
                f->glDepthFunc(GL_LESS);
                program_->setUniformValue("u_ndc_z_bias", 0.0f);
            }
        }
    };

    if (gridDrawPolicy.drawBeforeGeometry)
    {
        // Step 9: draw first when the user explicitly keeps the grid behind scene geometry.
        drawGridPass(gridDrawPolicy.depthTestScene);
    }

    if (canDrawSolid)
    {
        // Step 10: refresh dynamic sprite VBOs before the solid pass consumes triangle batches.
        if (spriteTrianglesDirty_
            && spriteTriVao_ && spriteTriVbo_)
        {
            rebuildSpriteVertices();
            spriteTriVao_->bind();
            spriteTriVbo_->bind();
            const std::size_t spriteByteCount = spriteTriVertices_.size() * sizeof(TriVertex);
            if (spriteByteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                spriteTriVertexCount_ = 0;
                spriteTrianglesDirty_ = false;
                spriteHasType0Billboards_ = false;
                for (TriBatch& batch : spriteTriBatches_)
                {
                    batch.firstVertex = 0;
                    batch.vertexCount = 0;
                }
                spriteTriVbo_->release();
                spriteTriVao_->release();
            }
            else
            {
                const int spriteBytes = static_cast<int>(spriteByteCount);
                if (spriteBytes <= 0)
                {
                    spriteTriVertexCount_ = 0;
                    spriteTriVbo_->release();
                    spriteTriVao_->release();
                    spriteTrianglesDirty_ = false;
                    spriteHasType0Billboards_ = false;
                    for (TriBatch& batch : spriteTriBatches_)
                    {
                        batch.firstVertex = 0;
                        batch.vertexCount = 0;
                    }
                }
                else
                {
                    if (spriteTriVboAllocatedBytes_ < spriteBytes)
                    {
                        const int half = spriteTriVboAllocatedBytes_ / 2;
                        const int grown = spriteTriVboAllocatedBytes_ + std::max(half, 4096);
                        const int newCap =
                            (spriteTriVboAllocatedBytes_ <= 0)
                                ? spriteBytes
                                : std::max(spriteBytes, grown);
                        spriteTriVbo_->allocate(nullptr, newCap);
                        spriteTriVboAllocatedBytes_ = newCap;
                    }
                    spriteTriVbo_->write(0, spriteTriVertices_.data(), spriteBytes);
                    spriteTriVbo_->release();
                    spriteTriVao_->release();
                }
            }
        }

        program_->setUniformValue("u_lighting_mode", static_cast<int>(lightingMode_));
        // Filled polys only: hide NotDrawn/Vertex/Wireframe regardless of pipeline.
        const int geoMin = static_cast<int>(TGeoMode::Solid);
        program_->setUniformValue("u_geo_min", geoMin);

        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
        f->glFrontFace(GL_CCW);

        f->glEnable(GL_POLYGON_OFFSET_FILL);
        // Keep the scene depth buffer at the real geometry depth. A positive
        // fill offset paired with a negative wire offset can exceed the real
        // separation between nearby sectors at grazing camera angles and let
        // a rear sector's wireframe pass through a solid foreground wall.
        f->glPolygonOffset(0.0f, 0.0f);

        program_->setUniformValue("u_color", QVector4D(1.00f, 1.00f, 1.00f, 1.0f));
        program_->setUniformValue("u_textured", 0);
        program_->setUniformValue("u_apply_classic_solid_light_ramp", false);
        program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
        program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));

        bool drewSortedSpriteTranslucency = false;

        auto drawTriBatches = [&](const std::vector<TriBatch>& batches, const bool translucentPass)
        {
            const bool wantInitialDepthWrite = !translucentPass;
            f->glDepthMask(wantInitialDepthWrite ? GL_TRUE : GL_FALSE);
            f->glEnable(GL_CULL_FACE);
            f->glCullFace(GL_BACK);
            f->glEnable(GL_POLYGON_OFFSET_FILL);
            program_->setUniformValue("u_ndc_z_bias", 0.0f);

            bool lastCullEnabled = true;
            bool lastDepthWrite = wantInitialDepthWrite;
            bool lastPolyOffsetEnabled = true;
            float lastNdcZBias = 0.0f;

            QVector3D cameraForward = target - eye;
            if (cameraForward.lengthSquared() <= sed::math::kTinyEpsilonF)
            {
                cameraForward = QVector3D(0.0f, 1.0f, 0.0f);
            }
            else
            {
                cameraForward.normalize();
            }
            const render::translucency::ProjectionKind translucencyProjection =
                camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic
                    ? render::translucency::ProjectionKind::orthographic
                    : render::translucency::ProjectionKind::perspective;

            auto cameraOrderDepthAt = [&](const QVector3D& world) -> float
            {
                const QVector3D rel = world - eye;
                return render::translucency::projectionOrderDepth(
                    rel.x(),
                    rel.y(),
                    rel.z(),
                    cameraForward.x(),
                    cameraForward.y(),
                    cameraForward.z(),
                    translucencyProjection);
            };

            const bool allowSortedThingTranslucency = true;

            std::vector<TriVertex> sortedVerts{};
            std::vector<std::pair<int, float>> sortedTris{}; // (triangle index, projection-aware depth proxy)
            auto uploadSortedThingVertices = [&](const std::vector<TriVertex>& verts) -> bool
            {
                const std::size_t byteCount = verts.size() * sizeof(TriVertex);
                if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    return false;
                }

                const int bytes = static_cast<int>(byteCount);
                translucentTriVbo_->bind();
                if (bytes <= 0)
                {
                    translucentTriVbo_->release();
                    return true;
                }
                else
                {
                    if (translucentTriVboAllocatedBytes_ < bytes)
                    {
                        const int half = translucentTriVboAllocatedBytes_ / 2;
                        const int grown = translucentTriVboAllocatedBytes_ + std::max(half, 4096);
                        const int newCap =
                            (translucentTriVboAllocatedBytes_ <= 0)
                                ? bytes
                                : std::max(bytes, grown);
                        translucentTriVbo_->allocate(nullptr, newCap);
                        translucentTriVboAllocatedBytes_ = newCap;
                    }
                    translucentTriVbo_->write(0, verts.data(), bytes);
                }
                translucentTriVbo_->release();
                return true;
            };

            auto drawSortedTranslucentThingBatch = [&](const TriBatch& batch) -> bool
            {
                if (!allowSortedThingTranslucency)
                {
                    return false;
                }
                if (batch.kind != TriKind::Thing)
                {
                    return false;
                }
                if (batch.vertexCount <= 0 || batch.firstVertex < 0)
                {
                    return false;
                }
                if ((batch.vertexCount % 3) != 0)
                {
                    return false;
                }

                const int first = batch.firstVertex;
                const int count = batch.vertexCount;
                if (first + count > static_cast<int>(triVertices_.size()))
                {
                    return false;
                }

                const int triCount = count / 3;
                if (triCount <= 1)
                {
                    return false;
                }

                const bool previewed = [&]() -> bool
                {
                    if (!transformPreviewActive_ || transformPreviewIdCount_ <= 0)
                    {
                        return false;
                    }

                    const int pid = static_cast<int>(triVertices_[static_cast<std::size_t>(first)].pickId + 0.5f);
                    if (pid <= 0)
                    {
                        return false;
                    }

                    for (int i = 0; i < transformPreviewIdCount_; ++i)
                    {
                        if (transformPreviewIds_[static_cast<std::size_t>(i)] == pid)
                        {
                            return true;
                        }
                    }

                    return false;
                }();

                sortedTris.clear();
                sortedTris.reserve(static_cast<std::size_t>(triCount));
                for (int i = 0; i < triCount; ++i)
                {
                    const TriVertex& v0 = triVertices_[static_cast<std::size_t>(first + (i * 3 + 0))];
                    const TriVertex& v1 = triVertices_[static_cast<std::size_t>(first + (i * 3 + 1))];
                    const TriVertex& v2 = triVertices_[static_cast<std::size_t>(first + (i * 3 + 2))];

                    QVector3D p0(v0.x, v0.y, v0.z);
                    QVector3D p1(v1.x, v1.y, v1.z);
                    QVector3D p2(v2.x, v2.y, v2.z);
                    if (previewed)
                    {
                        p0 = transformPreviewMatrix_.map(p0);
                        p1 = transformPreviewMatrix_.map(p1);
                        p2 = transformPreviewMatrix_.map(p2);
                    }

                    float depth = std::numeric_limits<float>::infinity();
                    depth = std::min(depth, cameraOrderDepthAt(p0));
                    depth = std::min(depth, cameraOrderDepthAt(p1));
                    depth = std::min(depth, cameraOrderDepthAt(p2));
                    sortedTris.emplace_back(i, depth);
                }

                // Match the other translucent-thing paths: use the nearest
                // projection-appropriate vertex depth rather than a triangle center.
                std::sort(sortedTris.begin(), sortedTris.end(), [](const auto& a, const auto& b)
                    {
                        if (a.second != b.second)
                        {
                            return a.second > b.second;
                        }
                        return a.first < b.first;
                    });

                sortedVerts.clear();
                sortedVerts.reserve(static_cast<std::size_t>(count));
                for (const auto& td : sortedTris)
                {
                    const int triIndex = td.first;
                    const int base = first + triIndex * 3;
                    sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 0)]);
                    sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 1)]);
                    sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 2)]);
                }

                if (sortedVerts.size() != static_cast<std::size_t>(count))
                {
                    return false;
                }

                translucentTriVao_->bind();
                if (!uploadSortedThingVertices(sortedVerts))
                {
                    translucentTriVao_->release();
                    triVao_->bind();
                    return false;
                }

                f->glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(sortedVerts.size()));

                translucentTriVao_->release();

                // Restore the main VAO binding (caller expects it to stay bound for subsequent batches).
                triVao_->bind();
                return true;
            };

            auto drawBatchRange = [&](const TriBatch& batch,
                                      const int firstVertex,
                                      const int vertexCount,
                                      const bool allowPerThingTriSort)
            {
                if (vertexCount <= 0)
                {
                    return;
                }

                if (batch.kind == TriKind::Level && !canDrawLevelSolid)
                {
                    return;
                }
                if (batch.kind == TriKind::Thing && !canDrawThingSolid)
                {
                    return;
                }

                if (projectedSky && batch.kind == TriKind::Level && batch.skyGroup != 0)
                {
                    return;
                }

                const bool isolatedSprite = batch.isSprite && level_
                    && property("spr.preview.isolated_resource").toBool();
                int previewGeometry = -1;
                if ( isolatedSprite )
                {
                    previewGeometry = property("spr.preview.geometry_mode").toInt();
                    if ( previewGeometry == 0 )
                    {
                        return;
                    }
                }
                const bool isolatedIjimSolid = isolatedSprite
                    && level_->kind == ProjectType::IJIM && previewGeometry == 3;
                bool textured = false;
                bool alphaTest = false;
                const bool iconCube =
                    (batch.kind == TriKind::Thing)
                    && std::string_view(batch.material).rfind(kThingIconMatPrefix, 0) == 0;
                const bool faceTranslucent = (batch.faceFlags & FF_Transluent) != 0u;
                const bool zWriteDisabled = (batch.faceFlags & FF_ZWriteDisabled) != 0u;
                const bool levelAdjoin = (batch.faceFlags & FF_RD_Adjoin) != 0u;
                const bool explicitTranslucent = faceTranslucent || zWriteDisabled;
                // Classic alpha-textured adjoins share the delayed translucent
                // queue even without the explicit face flag. Thing vertex and
                // multi-bit texture alpha retain their existing blend rules.
                bool textureHasAlpha = false;
                const ClassicSolidLightRamp* classicSolidLightRamp = nullptr;
                bool translucent = batch.kind == TriKind::Level
                    ? render::translucency::levelSurfaceUsesTranslucentPass(
                        levelAdjoin, faceTranslucent, zWriteDisabled, false)
                    : explicitTranslucent || (batch.minAlpha < 0.999f);
                QVector4D materialColor(1.0f, 1.0f, 1.0f, 1.0f);
                QVector2D uvScale(1.0f, 1.0f);
                QVector2D uvOffset(0.0f, 0.0f);

                if (showTextures_ && !isolatedIjimSolid)
                {
                    if (const auto it = textures_.find(batch.key); it != textures_.end())
                    {
                        const TextureInfo& info = it->second;
                        materialColor = QVector4D(info.solidColor, 1.0f);
                        if (level_
                            && level_->kind != ProjectType::IJIM
                            && info.classicSolidLightRamp)
                        {
                            classicSolidLightRamp =
                                &*info.classicSolidLightRamp;
                        }

                        if (info.texture)
                        {
                            textured = true;

                            if (batch.uvNormalized)
                            {
                                uvScale.setX(1.0f);
                                uvScale.setY(1.0f);
                            }
                            else if (info.width > 0 && info.height > 0)
                            {
                                uvScale.setX(1.0f / static_cast<float>(info.width));
                                uvScale.setY(1.0f / static_cast<float>(info.height));
                            }

                            textureHasAlpha = info.alphaBpp > 0;

                            if (batch.kind == TriKind::Level)
                            {
                                translucent = render::translucency::levelSurfaceUsesTranslucentPass(
                                    levelAdjoin,
                                    faceTranslucent,
                                    zWriteDisabled,
                                    textureHasAlpha);
                            }

                            // Things: treat multi-bit alpha textures as blended translucency (alphaBpp == 1 is cutout).
                            if (batch.kind == TriKind::Thing && !iconCube && info.alphaBpp > 1)
                            {
                                translucent = true;
                            }

                            // Cutout: 1-bit alpha or explicit alpha-ref flag.
                            const bool alphaRef = (batch.faceFlags & FF_RD_AlphaRef) != 0u;
                            alphaTest = textureHasAlpha && ((info.alphaBpp == 1) || alphaRef);

                            // Sprites are billboard effects in this renderer; alpha-test thresholding on
                            // 1-bit/ indexed alpha causes angle/LOD pop. Prefer blended sprite alpha.
                            if (batch.isSprite && !iconCube && textureHasAlpha)
                            {
                                translucent = true;
                                alphaTest = false;
                            }

                            info.texture->bind(0);
                        }
                    }
                }

                program_->setUniformValue("u_color", materialColor);
                program_->setUniformValue(
                    "u_apply_classic_solid_light_ramp",
                    classicSolidLightRamp != nullptr);
                if (classicSolidLightRamp)
                {
                    program_->setUniformValueArray(
                        "u_classic_solid_light_ramp",
                        classicSolidLightRamp->data(),
                        static_cast<int>(classicSolidLightRamp->size()));
                }

                // Match original sprite path: sprites are alpha-queue primitives
                // (rendered in translucent pass with z-write disabled behavior).
                if (batch.isSprite && !iconCube)
                {
                    translucent = true;
                    alphaTest = false;
                }

                if (translucent != translucentPass)
                {
                    return;
                }

                const bool useCameraSectorTint = level_
                    && render::classicBatchUsesCameraSectorTint(
                        level_->kind,
                        renderPipeline_ == RenderPipeline::GamePreview,
                        batch.kind == TriKind::Level,
                        batch.faceFlags);
                program_->setUniformValue(
                    "u_apply_classic_sector_tint",
                    useCameraSectorTint);
                program_->setUniformValue(
                    "u_apply_local_classic_sector_tint",
                    level_
                        && render::classicBatchUsesLocalSectorTint(
                            level_->kind,
                            renderPipeline_ == RenderPipeline::GamePreview,
                            batch.kind == TriKind::Level,
                            batch.faceFlags));

                const bool depthWrite = !translucentPass && ((batch.faceFlags & FF_ZWriteDisabled) == 0);
                if (depthWrite != lastDepthWrite)
                {
                    f->glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
                    lastDepthWrite = depthWrite;
                }

                // Keep sprites out of the level-surface polygon-offset state.
                // The current level offset is deliberately zero so the depth
                // buffer remains an exact occluder for late overlays; retaining
                // the batch distinction keeps this path safe if other passes
                // ever opt into a nonzero offset locally.
                const bool wantPolyOffset = !batch.isSprite;
                if (wantPolyOffset != lastPolyOffsetEnabled)
                {
                    if (wantPolyOffset)
                    {
                        f->glEnable(GL_POLYGON_OFFSET_FILL);
                    }
                    else
                    {
                        f->glDisable(GL_POLYGON_OFFSET_FILL);
                    }
                    lastPolyOffsetEnabled = wantPolyOffset;
                }

                // Editor shows a classic translucent portal from either side
                // for convenient inspection through its NotDrawn counterpart.
                // Game Preview retains engine culling.
                const bool showClassicAdjoinBackface = level_
                    && render::classicEditorShowsTranslucentAdjoinBackface(
                        level_->kind,
                        renderPipeline_ == RenderPipeline::GamePreview,
                        batch.kind == TriKind::Level,
                        batch.faceFlags,
                        textureHasAlpha);
                // Sprites are always rendered double-sided in the original renderer.
                const bool wantCull = !batch.isSprite
                    && !showClassicAdjoinBackface
                    && ((batch.kind != TriKind::Thing) || !batch.doubleSided);
                if (wantCull != lastCullEnabled)
                {
                    if (wantCull)
                    {
                        f->glEnable(GL_CULL_FACE);
                        f->glCullFace(GL_BACK);
                    }
                    else
                    {
                        f->glDisable(GL_CULL_FACE);
                    }
                    lastCullEnabled = wantCull;
                }

                // Billboards can be nearly coplanar with world geometry (e.g. glow sprites on walls),
                // which causes angle/pitch-dependent depth fighting. Apply a tiny clip-space depth lift
                // only in perspective; in orthographic projection the same NDC shift maps to a large
                // world-space delta across the whole near/far range and can pull sprites through walls.
                constexpr float kSpriteNdcZBias = 2.0e-4f;
                const bool orthoProjection =
                    camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
                const float wantNdcZBias = (batch.isSprite && !orthoProjection) ? kSpriteNdcZBias : 0.0f;
                if (std::abs(wantNdcZBias - lastNdcZBias) > 1.0e-9f)
                {
                    program_->setUniformValue("u_ndc_z_bias", wantNdcZBias);
                    lastNdcZBias = wantNdcZBias;
                }

                program_->setUniformValue("u_textured", textured ? 1 : 0);
                program_->setUniformValue("u_alpha_test", alphaTest ? 1 : 0);
                constexpr float kAlphaCutoutRef = 0.5f;
                program_->setUniformValue("u_alpha_ref", alphaTest ? kAlphaCutoutRef : 0.01f);
                program_->setUniformValue("u_uv_scale", uvScale);
                program_->setUniformValue("u_uv_offset", uvOffset);
                // IJIM's global Solid route omits the texture and keeps the
                // Sprite's flat extra-light color, without editor cube shading.
                program_->setUniformValue("u_flat_shade",
                    batch.kind == TriKind::Thing && (!textured || iconCube) && !isolatedIjimSolid);
                const bool fogEnabledForBatch =
                    level_
                    && batch.skyGroup == 0
                    && shouldRenderIjimFog(level_->kind, level_->header.fog, showFog_, batch.faceFlags);
                program_->setUniformValue("u_fog_enabled", fogEnabledForBatch);

                // Per-triangle sorting for translucent 3DO batches: improves alpha order when a model has many
                // overlapping translucent faces (batch-center sorting is often insufficient).
                const bool fullBatch = (firstVertex == batch.firstVertex) && (vertexCount == batch.vertexCount);
                if (allowPerThingTriSort && translucentPass && batch.kind == TriKind::Thing && fullBatch
                    && !isolatedSprite)
                {
                    if (drawSortedTranslucentThingBatch(batch))
                    {
                        return;
                    }
                }

                const bool previewCornerOrWire = previewGeometry == 1 || previewGeometry == 2;
                if ( previewCornerOrWire )
                {
                    program_->setUniformValue("u_sprite_preview_geometry", previewGeometry);
                }
                f->glDrawArrays(GL_TRIANGLES, firstVertex, vertexCount);
                if ( previewCornerOrWire )
                {
                    // Subsequent overlays and every ordinary scene draw must
                    // retain their own geometry/shading policy.
                    program_->setUniformValue("u_sprite_preview_geometry", -1);
                }
            };

            auto drawOneBatch = [&](const TriBatch& batch)
            {
                drawBatchRange(batch, batch.firstVertex, batch.vertexCount, true);
            };

            if (!translucentPass)
            {
                for (const TriBatch& batch : batches)
                {
                    drawOneBatch(batch);
                }
            }
            else
            {
                struct DrawItem
                {
                    const TriBatch* batch{};
                    int firstVertex{};
                    int vertexCount{};
                    float depthKey{};
                    int stableKey{};
                    bool drawLast{};
                };

                // For large translucent batches, use the nearest bounds corner as a stable fallback proxy.
                auto batchDepthKey = [&](const TriBatch& batch) -> float
                {
                    if (batch.isSprite)
                    {
                        // Billboard sprites are perceived as one effect card rather than a
                        // volumetric box. Sorting them by the nearest bounds corner makes
                        // large distant sprites shuffle in order as the camera rotates.
                        return cameraOrderDepthAt(batch.boundsCenter);
                    }

                    const QVector3D mn = batch.boundsMin;
                    const QVector3D mx = batch.boundsMax;

                    const QVector3D corners[8] = {
                        QVector3D(mn.x(), mn.y(), mn.z()),
                        QVector3D(mx.x(), mn.y(), mn.z()),
                        QVector3D(mn.x(), mx.y(), mn.z()),
                        QVector3D(mx.x(), mx.y(), mn.z()),
                        QVector3D(mn.x(), mn.y(), mx.z()),
                        QVector3D(mx.x(), mn.y(), mx.z()),
                        QVector3D(mn.x(), mx.y(), mx.z()),
                        QVector3D(mx.x(), mx.y(), mx.z()),
                    };

                    float nearest = std::numeric_limits<float>::infinity();
                    for (const QVector3D& c : corners)
                    {
                        const float d = cameraOrderDepthAt(c);
                        if (!std::isfinite(d))
                        {
                            continue;
                        }
                        nearest = std::min(nearest, d);
                    }

                    if (std::isfinite(nearest))
                    {
                        return nearest;
                    }

                    return cameraOrderDepthAt(batch.boundsCenter);
                };

                auto batchIsTranslucent = [&](const TriBatch& batch) -> bool
                {
                    const bool faceTranslucent = (batch.faceFlags & FF_Transluent) != 0u;
                    const bool zWriteDisabled = (batch.faceFlags & FF_ZWriteDisabled) != 0u;
                    const bool explicitTranslucent = faceTranslucent || zWriteDisabled;
                    if (batch.kind == TriKind::Level)
                    {
                        bool textureHasAlpha = false;
                        if (showTextures_)
                        {
                            if (const auto itTex = textures_.find(batch.key);
                                itTex != textures_.end() && itTex->second.texture)
                            {
                                textureHasAlpha = itTex->second.alphaBpp > 0;
                            }
                        }
                        return render::translucency::levelSurfaceUsesTranslucentPass(
                            (batch.faceFlags & FF_RD_Adjoin) != 0u,
                            faceTranslucent,
                            zWriteDisabled,
                            textureHasAlpha);
                    }

                    if (batch.isSprite)
                    {
                        return true;
                    }

                    bool translucent = explicitTranslucent || (batch.minAlpha < 0.999f);
                    if (!translucent && showTextures_)
                    {
                        const bool iconCube =
                            (batch.kind == TriKind::Thing)
                            && std::string_view(batch.material).rfind(kThingIconMatPrefix, 0) == 0;
                        if (!iconCube)
                        {
                            if (const auto itTex = textures_.find(batch.key);
                                itTex != textures_.end() && itTex->second.texture)
                            {
                                const TextureInfo& info = itTex->second;
                                if (info.alphaBpp > 1)
                                {
                                    translucent = true;
                                }
                            }
                        }
                    }

                    return translucent;
                };

                if (allowSortedThingTranslucency)
                {
                    struct ThingTriRef
                    {
                        const TriBatch* batch{};
                        int baseVertex{};
                        float depthKey{};
                    };

                    struct ThingData
                    {
                        std::vector<ThingTriRef> tris{};
                        float depthKey{std::numeric_limits<float>::infinity()};
                        bool hasDepth{};
                    };

                    std::unordered_map<int, ThingData> thingTris{};
                    std::vector<DrawItem> levelItems{};
                    std::vector<DrawItem> unsortedThingItems{};
                    levelItems.reserve(batches.size());
                    unsortedThingItems.reserve(batches.size());

                    auto levelBatchDepthKey = [&](const TriBatch& batch) -> float
                    {
                        const QVector3D mn = batch.boundsMin;
                        const QVector3D mx = batch.boundsMax;

                        const QVector3D corners[8] = {
                            QVector3D(mn.x(), mn.y(), mn.z()),
                            QVector3D(mx.x(), mn.y(), mn.z()),
                            QVector3D(mn.x(), mx.y(), mn.z()),
                            QVector3D(mx.x(), mx.y(), mn.z()),
                            QVector3D(mn.x(), mn.y(), mx.z()),
                            QVector3D(mx.x(), mn.y(), mx.z()),
                            QVector3D(mn.x(), mx.y(), mx.z()),
                            QVector3D(mx.x(), mx.y(), mx.z()),
                        };

                        float nearest = std::numeric_limits<float>::infinity();
                        for (const QVector3D& c : corners)
                        {
                            const float d = cameraOrderDepthAt(c);
                            if (!std::isfinite(d))
                            {
                                continue;
                            }
                            nearest = std::min(nearest, d);
                        }

                        if (std::isfinite(nearest))
                        {
                            return nearest;
                        }

                        return cameraOrderDepthAt(batch.boundsCenter);
                    };

                    auto isPreviewedPid = [&](const int pid) -> bool
                    {
                        if (!transformPreviewActive_ || transformPreviewIdCount_ <= 0 || pid <= 0)
                        {
                            return false;
                        }

                        for (int i = 0; i < transformPreviewIdCount_; ++i)
                        {
                            if (transformPreviewIds_[static_cast<std::size_t>(i)] == pid)
                            {
                                return true;
                            }
                        }
                        return false;
                    };

                    for (const TriBatch& batch : batches)
                    {
                        if (batch.vertexCount <= 0)
                        {
                            continue;
                        }
                        if (batch.kind == TriKind::Level && !canDrawLevelSolid)
                        {
                            continue;
                        }
                        if (batch.kind == TriKind::Thing && !canDrawThingSolid)
                        {
                            continue;
                        }
                        if (projectedSky && batch.kind == TriKind::Level && batch.skyGroup != 0)
                        {
                            continue;
                        }

                        if (!batchIsTranslucent(batch))
                        {
                            continue;
                        }

                        if (batch.kind == TriKind::Thing)
                        {
                            if (batch.firstVertex < 0 || (batch.vertexCount % 3) != 0)
                            {
                                continue;
                            }

                            const int first = batch.firstVertex;
                            const int count = batch.vertexCount;
                            if (first + count > static_cast<int>(triVertices_.size()))
                            {
                                continue;
                            }

                            int fallbackRunStart = -1;
                            int fallbackRunPid = 0;
                            QVector3D fallbackBoundsMin(
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max());
                            QVector3D fallbackBoundsMax(
                                -std::numeric_limits<float>::max(),
                                -std::numeric_limits<float>::max(),
                                -std::numeric_limits<float>::max());
                            bool fallbackHasBounds = false;

                            auto flushFallbackRun = [&](const int runEnd)
                            {
                                if (fallbackRunStart < 0)
                                {
                                    return;
                                }

                                const int runCount = runEnd - fallbackRunStart;
                                if (runCount <= 0)
                                {
                                    fallbackRunStart = -1;
                                    fallbackHasBounds = false;
                                    return;
                                }

                                const float depthKey =
                                    fallbackHasBounds
                                        ? cameraOrderDepthAt((fallbackBoundsMin + fallbackBoundsMax) * 0.5f)
                                        : -1e9f;
                                const int stableKey =
                                    (fallbackRunPid > 0) ? fallbackRunPid : fallbackRunStart;
                                unsortedThingItems.push_back(
                                    DrawItem{
                                        .batch       = &batch,
                                        .firstVertex = fallbackRunStart,
                                        .vertexCount = runCount,
                                        .depthKey    = depthKey,
                                        .stableKey   = stableKey,
                                    });

                                fallbackRunStart = -1;
                                fallbackHasBounds = false;
                            };

                            for (int ofs = 0; ofs < count; ofs += 3)
                            {
                                const int base = first + ofs;
                                const TriVertex& v0 = triVertices_[static_cast<std::size_t>(base + 0)];
                                const TriVertex& v1 = triVertices_[static_cast<std::size_t>(base + 1)];
                                const TriVertex& v2 = triVertices_[static_cast<std::size_t>(base + 2)];

                                const int pid = static_cast<int>(v0.pickId + 0.5f);

                                QVector3D p0(v0.x, v0.y, v0.z);
                                QVector3D p1(v1.x, v1.y, v1.z);
                                QVector3D p2(v2.x, v2.y, v2.z);
                                if (isPreviewedPid(pid))
                                {
                                    p0 = transformPreviewMatrix_.map(p0);
                                    p1 = transformPreviewMatrix_.map(p1);
                                    p2 = transformPreviewMatrix_.map(p2);
                                }

                                float depth = std::numeric_limits<float>::infinity();
                                depth = std::min(depth, cameraOrderDepthAt(p0));
                                depth = std::min(depth, cameraOrderDepthAt(p1));
                                depth = std::min(depth, cameraOrderDepthAt(p2));

                                if (pid <= 0)
                                {
                                    if (fallbackRunStart < 0)
                                    {
                                        fallbackRunStart = base;
                                        fallbackRunPid = pid;
                                        fallbackBoundsMin = QVector3D(
                                            std::numeric_limits<float>::max(),
                                            std::numeric_limits<float>::max(),
                                            std::numeric_limits<float>::max());
                                        fallbackBoundsMax = QVector3D(
                                            -std::numeric_limits<float>::max(),
                                            -std::numeric_limits<float>::max(),
                                            -std::numeric_limits<float>::max());
                                        fallbackHasBounds = false;
                                    }

                                    const auto expandFallbackBounds = [&](const QVector3D& p)
                                    {
                                        fallbackBoundsMin.setX(std::min(fallbackBoundsMin.x(), p.x()));
                                        fallbackBoundsMin.setY(std::min(fallbackBoundsMin.y(), p.y()));
                                        fallbackBoundsMin.setZ(std::min(fallbackBoundsMin.z(), p.z()));
                                        fallbackBoundsMax.setX(std::max(fallbackBoundsMax.x(), p.x()));
                                        fallbackBoundsMax.setY(std::max(fallbackBoundsMax.y(), p.y()));
                                        fallbackBoundsMax.setZ(std::max(fallbackBoundsMax.z(), p.z()));
                                        fallbackHasBounds = true;
                                    };
                                    expandFallbackBounds(p0);
                                    expandFallbackBounds(p1);
                                    expandFallbackBounds(p2);
                                    continue;
                                }

                                flushFallbackRun(base);

                                ThingData& td = thingTris[pid];
                                td.tris.push_back(ThingTriRef{ &batch, base, depth });
                                if (std::isfinite(depth))
                                {
                                    td.depthKey = td.hasDepth ? std::min(td.depthKey, depth) : depth;
                                    td.hasDepth = true;
                                }
                            }

                            flushFallbackRun(first + count);

                            continue;
                        }

                        // Keep translucent level surfaces on their original batch grouping here.
                        // The preview pipeline already gives translucent adjoins unique batches, and the
                        // later per-surface split introduced obvious water-plane seams in game preview.
                        if (batch.firstVertex < 0 || batch.vertexCount <= 0)
                        {
                            continue;
                        }

                        if (batch.firstVertex + batch.vertexCount > static_cast<int>(triVertices_.size()))
                        {
                            continue;
                        }

                        levelItems.push_back(
                            DrawItem{
                                .batch       = &batch,
                                .firstVertex = batch.firstVertex,
                                .vertexCount = batch.vertexCount,
                                .depthKey    = levelBatchDepthKey(batch),
                                .stableKey   = batch.firstVertex,
                            });
                    }

                    // Collect translucent sprite runs per instance (pick-id) so they can be sorted against
                    // translucent level geometry using the same nearest-distance policy as the legacy alpha queue.
                    std::vector<DrawItem> spriteItems{};
                    spriteItems.reserve(spriteTriBatches_.size() * 4);

                    for (const TriBatch& batch : spriteTriBatches_)
                    {
                        if (batch.vertexCount <= 0)
                        {
                            continue;
                        }
                        if (batch.kind != TriKind::Thing || !canDrawThingSolid)
                        {
                            continue;
                        }

                        if (!batchIsTranslucent(batch))
                        {
                            continue;
                        }

                        if (batch.firstVertex < 0 || (batch.vertexCount % 3) != 0)
                        {
                            continue;
                        }

                        const int first = batch.firstVertex;
                        const int count = batch.vertexCount;
                        if (first + count > static_cast<int>(spriteTriVertices_.size()))
                        {
                            continue;
                        }

                        int runStart = first;
                        int runPid = static_cast<int>(spriteTriVertices_[static_cast<std::size_t>(first)].pickId + 0.5f);
                        bool runPreviewed = isPreviewedPid(runPid);
                        QVector3D runBoundsMin(
                            std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max());
                        QVector3D runBoundsMax(
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max());
                        bool runHasBounds = false;

                        auto flushRun = [&](const int runEnd)
                        {
                            const int runCount = runEnd - runStart;
                            if (runCount <= 0)
                            {
                                return;
                            }

                            const float depthKey =
                                runHasBounds
                                ? cameraOrderDepthAt((runBoundsMin + runBoundsMax) * 0.5f)
                                : -1e9f;
                            const int stableKey = (runPid > 0) ? runPid : runStart;

                            spriteItems.push_back(DrawItem{
                                .batch       = &batch,
                                .firstVertex = runStart,
                                .vertexCount = runCount,
                                .depthKey    = depthKey,
                                .stableKey   = stableKey,
                            });
                        };

                        for (int ofs = 0; ofs < count; ofs += 3)
                        {
                            const int base = first + ofs;
                            const TriVertex& v0 = spriteTriVertices_[static_cast<std::size_t>(base + 0)];
                            const TriVertex& v1 = spriteTriVertices_[static_cast<std::size_t>(base + 1)];
                            const TriVertex& v2 = spriteTriVertices_[static_cast<std::size_t>(base + 2)];

                            const int pid = static_cast<int>(v0.pickId + 0.5f);
                            if (pid != runPid)
                            {
                                flushRun(base);

                                runStart = base;
                                runPid = pid;
                                runPreviewed = isPreviewedPid(runPid);
                                runBoundsMin = QVector3D(
                                    std::numeric_limits<float>::max(),
                                    std::numeric_limits<float>::max(),
                                    std::numeric_limits<float>::max());
                                runBoundsMax = QVector3D(
                                    -std::numeric_limits<float>::max(),
                                    -std::numeric_limits<float>::max(),
                                    -std::numeric_limits<float>::max());
                                runHasBounds = false;
                            }

                            QVector3D p0(v0.x, v0.y, v0.z);
                            QVector3D p1(v1.x, v1.y, v1.z);
                            QVector3D p2(v2.x, v2.y, v2.z);
                            if (runPreviewed)
                            {
                                p0 = transformPreviewMatrix_.map(p0);
                                p1 = transformPreviewMatrix_.map(p1);
                                p2 = transformPreviewMatrix_.map(p2);
                            }

                            // Keep billboard/translucent sprite sorting anchored to the
                            // sprite run's actual bounds center. Averaging submitted
                            // triangle vertices biases the depth proxy because quad
                            // corners are duplicated unevenly across the two triangles.
                            const auto expandRunBounds = [&](const QVector3D& p)
                            {
                                runBoundsMin.setX(std::min(runBoundsMin.x(), p.x()));
                                runBoundsMin.setY(std::min(runBoundsMin.y(), p.y()));
                                runBoundsMin.setZ(std::min(runBoundsMin.z(), p.z()));
                                runBoundsMax.setX(std::max(runBoundsMax.x(), p.x()));
                                runBoundsMax.setY(std::max(runBoundsMax.y(), p.y()));
                                runBoundsMax.setZ(std::max(runBoundsMax.z(), p.z()));
                                runHasBounds = true;
                            };
                            expandRunBounds(p0);
                            expandRunBounds(p1);
                            expandRunBounds(p2);
                        }

                        flushRun(first + count);
                    }

                    // Sort triangles within each thing (far->near).
                    std::vector<std::pair<int, float>> thingKeys{};
                    thingKeys.reserve(thingTris.size());
                    for (auto& kv : thingTris)
                    {
                        auto& td = kv.second;
                        if (!td.hasDepth)
                        {
                            td.depthKey = -1e9f;
                        }
                        if (td.tris.size() > 1)
                        {
                            std::sort(td.tris.begin(), td.tris.end(), [](const ThingTriRef& a, const ThingTriRef& b)
                                {
                                    if (a.depthKey != b.depthKey)
                                    {
                                        return a.depthKey > b.depthKey;
                                    }
                                    return a.baseVertex < b.baseVertex;
                                });
                        }
                        thingKeys.emplace_back(kv.first, td.depthKey);
                    }

                    // Merge translucent level batches and per-thing translucent triangles into a single draw order.
                    struct AnyItem
                    {
                        const TriBatch* batch{};
                        int firstVertex{};
                        int vertexCount{};
                        int pid{};
                        float depthKey{};
                        bool isThing{};
                        bool isSprite{};
                        int stableKey{};
                    };

                    std::vector<AnyItem> drawList{};
                    drawList.reserve(levelItems.size() + unsortedThingItems.size() + spriteItems.size() + thingKeys.size());

                    for (const DrawItem& it : levelItems)
                    {
                        drawList.push_back(AnyItem{
                            .batch       = it.batch,
                            .firstVertex = it.firstVertex,
                            .vertexCount = it.vertexCount,
                            .pid         = 0,
                            .depthKey    = it.depthKey,
                            .isThing     = false,
                            .isSprite    = false,
                            .stableKey   = it.stableKey,
                        });
                    }
                    for (const DrawItem& it : unsortedThingItems)
                    {
                        drawList.push_back(AnyItem{
                            .batch       = it.batch,
                            .firstVertex = it.firstVertex,
                            .vertexCount = it.vertexCount,
                            .pid         = 0,
                            .depthKey    = it.depthKey,
                            .isThing     = false,
                            .isSprite    = false,
                            .stableKey   = it.stableKey,
                        });
                    }
                    for (const DrawItem& it : spriteItems)
                    {
                        drawList.push_back(AnyItem{
                            .batch       = it.batch,
                            .firstVertex = it.firstVertex,
                            .vertexCount = it.vertexCount,
                            .pid         = 0,
                            .depthKey    = it.depthKey,
                            .isThing     = true,
                            .isSprite    = true,
                            .stableKey   = it.stableKey,
                        });
                    }
                    for (const auto& tk : thingKeys)
                    {
                        drawList.push_back(AnyItem{
                            .batch       = nullptr,
                            .firstVertex = 0,
                            .vertexCount = 0,
                            .pid         = tk.first,
                            .depthKey    = tk.second,
                            .isThing     = true,
                            .isSprite    = false,
                            .stableKey   = tk.first,
                        });
                    }

                    std::stable_sort(drawList.begin(), drawList.end(), [](const AnyItem& a, const AnyItem& b)
                        {
                            const auto makeKey = [](const AnyItem& item)
                            {
                                render::translucency::PrimitiveKind kind =
                                    render::translucency::PrimitiveKind::level;
                                if (item.isSprite)
                                {
                                    kind = render::translucency::PrimitiveKind::sprite;
                                }
                                else if (item.isThing || (item.batch && item.batch->kind == TriKind::Thing))
                                {
                                    kind = render::translucency::PrimitiveKind::thing;
                                }
                                return render::translucency::OrderKey{
                                    .depth = item.depthKey,
                                    .kind = kind,
                                    .stableKey = item.stableKey,
                                    .firstVertex = item.batch ? item.firstVertex : -1,
                                };
                            };
                            return render::translucency::backToFrontLess(makeKey(a), makeKey(b));
                        });

                    auto drawSortedThing = [&](const int pid)
                    {
                        const auto it = thingTris.find(pid);
                        if (it == thingTris.end() || it->second.tris.empty())
                        {
                            return;
                        }

                        const auto& tris = it->second.tris;

                        const TriBatch* currentBatch{};
                        sortedVerts.clear();
                        std::vector<int> currentBatchTriBases{};
                        currentBatchTriBases.reserve(tris.size());

                        auto flush = [&]()
                        {
                            if (!currentBatch || sortedVerts.empty())
                            {
                                currentBatchTriBases.clear();
                                return;
                            }

                            bool textured = false;
                            bool alphaTest = false;
                            QVector4D materialColor(
                                1.0f, 1.0f, 1.0f, 1.0f);
                            const ClassicSolidLightRamp* classicSolidLightRamp =
                                nullptr;
                            QVector2D uvScale(1.0f, 1.0f);
                            QVector2D uvOffset(0.0f, 0.0f);
                            const bool iconCube =
                                std::string_view(currentBatch->material).rfind(kThingIconMatPrefix, 0) == 0;

                            if (showTextures_)
                            {
                                if (const auto itTex = textures_.find(currentBatch->key); itTex != textures_.end() && itTex->second.texture)
                                {
                                    const TextureInfo& info = itTex->second;
                                    textured = true;
                                    materialColor = QVector4D(
                                        info.solidColor, 1.0f);
                                    if (level_
                                        && level_->kind != ProjectType::IJIM
                                        && info.classicSolidLightRamp)
                                    {
                                        classicSolidLightRamp =
                                            &*info.classicSolidLightRamp;
                                    }

                                    if (currentBatch->uvNormalized)
                                    {
                                        uvScale = QVector2D(1.0f, 1.0f);
                                    }
                                    else if (info.width > 0 && info.height > 0)
                                    {
                                        uvScale = QVector2D(1.0f / static_cast<float>(info.width), 1.0f / static_cast<float>(info.height));
                                    }

                                    const bool haveAlpha = info.alphaBpp > 0;
                                    const bool alphaRef = (currentBatch->faceFlags & FF_RD_AlphaRef) != 0u;
                                    alphaTest = haveAlpha && ((info.alphaBpp == 1) || alphaRef);
                                    info.texture->bind(0);
                                }
                            }

                            if (!lastPolyOffsetEnabled)
                            {
                                f->glEnable(GL_POLYGON_OFFSET_FILL);
                                lastPolyOffsetEnabled = true;
                            }

                            const bool wantCull = !currentBatch->doubleSided;
                            if (wantCull != lastCullEnabled)
                            {
                                if (wantCull)
                                {
                                    f->glEnable(GL_CULL_FACE);
                                    f->glCullFace(GL_BACK);
                                }
                                else
                                {
                                    f->glDisable(GL_CULL_FACE);
                                }
                                lastCullEnabled = wantCull;
                            }

                            program_->setUniformValue("u_textured", textured ? 1 : 0);
                            program_->setUniformValue("u_color", materialColor);
                            program_->setUniformValue(
                                "u_apply_classic_solid_light_ramp",
                                classicSolidLightRamp != nullptr);
                            if (classicSolidLightRamp)
                            {
                                program_->setUniformValueArray(
                                    "u_classic_solid_light_ramp",
                                    classicSolidLightRamp->data(),
                                    static_cast<int>(
                                        classicSolidLightRamp->size()));
                            }
                            program_->setUniformValue("u_alpha_test", alphaTest ? 1 : 0);
                            constexpr float kAlphaCutoutRef = 0.5f;
                            program_->setUniformValue("u_alpha_ref", alphaTest ? kAlphaCutoutRef : 0.01f);
                            if (std::abs(lastNdcZBias) > 1.0e-9f)
                            {
                                program_->setUniformValue("u_ndc_z_bias", 0.0f);
                                lastNdcZBias = 0.0f;
                            }
                            program_->setUniformValue("u_uv_scale", uvScale);
                            program_->setUniformValue("u_uv_offset", uvOffset);
                            program_->setUniformValue("u_flat_shade", !textured || iconCube);
                            const bool fogEnabledForBatch =
                                level_
                                && currentBatch->skyGroup == 0
                                && shouldRenderIjimFog(level_->kind, level_->header.fog, showFog_, currentBatch->faceFlags);
                            program_->setUniformValue("u_fog_enabled", fogEnabledForBatch);

                            translucentTriVao_->bind();
                            if (!uploadSortedThingVertices(sortedVerts))
                            {
                                translucentTriVao_->release();
                                triVao_->bind();

                                // Fall back to the original tri buffer so a transient sorted-upload
                                // failure doesn't make this translucent thing batch disappear.
                                for (const int base : currentBatchTriBases)
                                {
                                    if (base < 0 || base + 2 >= static_cast<int>(triVertices_.size()))
                                    {
                                        continue;
                                    }
                                    f->glDrawArrays(GL_TRIANGLES, base, 3);
                                }

                                sortedVerts.clear();
                                currentBatchTriBases.clear();
                                return;
                            }
                            f->glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(sortedVerts.size()));
                            translucentTriVao_->release();

                            // Restore expected binding for the caller.
                            triVao_->bind();
                            currentBatchTriBases.clear();
                        };

                        for (const ThingTriRef& tr : tris)
                        {
                            if (tr.batch != currentBatch)
                            {
                                flush();
                                currentBatch = tr.batch;
                                sortedVerts.clear();
                                currentBatchTriBases.clear();
                            }

                            const int base = tr.baseVertex;
                            if (base + 2 >= static_cast<int>(triVertices_.size()))
                            {
                                continue;
                            }

                            sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 0)]);
                            sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 1)]);
                            sortedVerts.push_back(triVertices_[static_cast<std::size_t>(base + 2)]);
                            currentBatchTriBases.push_back(base);
                        }

                        flush();
                    };

                    bool triBound = true;
                    bool spriteBound = false;
                    for (const AnyItem& item : drawList)
                    {
                        if (item.isThing && !item.isSprite)
                        {
                            if (spriteBound && spriteTriVao_)
                            {
                                spriteTriVao_->release();
                                spriteBound = false;
                            }
                            if (!triBound)
                            {
                                triVao_->bind();
                                triBound = true;
                            }
                            drawSortedThing(item.pid);
                            continue;
                        }

                        if (item.batch)
                        {
                            if (item.isSprite)
                            {
                                if (!spriteTriVao_ || spriteTriVertexCount_ <= 0)
                                {
                                    continue;
                                }

                                if (!spriteBound)
                                {
                                    if (triBound)
                                    {
                                        triVao_->release();
                                        triBound = false;
                                    }
                                    spriteTriVao_->bind();
                                    spriteBound = true;
                                }

                                drawBatchRange(*item.batch, item.firstVertex, item.vertexCount, false);
                            }
                            else
                            {
                                if (!triBound)
                                {
                                    if (spriteBound)
                                    {
                                        spriteTriVao_->release();
                                        spriteBound = false;
                                    }
                                    triVao_->bind();
                                    triBound = true;
                                }

                                drawBatchRange(*item.batch, item.firstVertex, item.vertexCount, true);
                            }
                        }
                    }

                    if (spriteBound && spriteTriVao_)
                    {
                        spriteTriVao_->release();
                    }
                    if (!triBound && triVao_)
                    {
                        triVao_->bind();
                    }

                    if (!spriteItems.empty())
                    {
                        drewSortedSpriteTranslucency = true;
                    }
                }
                else
                {
                    std::vector<DrawItem> items{};
                    items.reserve(batches.size());
                    const bool spriteBatchCollection = (&batches == &spriteTriBatches_);

                    auto isPreviewedPid = [&](const int pid) -> bool
                    {
                        if (!transformPreviewActive_ || transformPreviewIdCount_ <= 0 || pid <= 0)
                        {
                            return false;
                        }

                        for (int i = 0; i < transformPreviewIdCount_; ++i)
                        {
                            if (transformPreviewIds_[static_cast<std::size_t>(i)] == pid)
                            {
                                return true;
                            }
                        }
                        return false;
                    };

                    for (const TriBatch& batch : batches)
                    {
                        if (batch.vertexCount <= 0)
                        {
                            continue;
                        }
                        if (batch.kind == TriKind::Level && !canDrawLevelSolid)
                        {
                            continue;
                        }
                        if (batch.kind == TriKind::Thing && !canDrawThingSolid)
                        {
                            continue;
                        }
                        if (projectedSky && batch.kind == TriKind::Level && batch.skyGroup != 0)
                        {
                            continue;
                        }

                        if (!batchIsTranslucent(batch))
                        {
                            continue;
                        }

                        if (spriteBatchCollection)
                        {
                            if (batch.firstVertex < 0 || (batch.vertexCount % 3) != 0)
                            {
                                continue;
                            }

                            const int first = batch.firstVertex;
                            const int count = batch.vertexCount;
                            if (first + count > static_cast<int>(spriteTriVertices_.size()))
                            {
                                continue;
                            }

                            int runStart = first;
                            int runPid = static_cast<int>(spriteTriVertices_[static_cast<std::size_t>(first)].pickId + 0.5f);
                            bool runPreviewed = isPreviewedPid(runPid);
                            QVector3D runBoundsMin(
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max());
                            QVector3D runBoundsMax(
                                -std::numeric_limits<float>::max(),
                                -std::numeric_limits<float>::max(),
                                -std::numeric_limits<float>::max());
                            bool runHasBounds = false;

                            auto flushRun = [&](const int runEnd)
                            {
                                const int runCount = runEnd - runStart;
                                if (runCount <= 0)
                                {
                                    return;
                                }

                                const float depthKey =
                                    runHasBounds
                                    ? cameraOrderDepthAt((runBoundsMin + runBoundsMax) * 0.5f)
                                    : -1e9f;
                                const int stableKey = (runPid > 0) ? runPid : runStart;

                                items.push_back(DrawItem{
                                    .batch       = &batch,
                                    .firstVertex = runStart,
                                    .vertexCount = runCount,
                                    .depthKey    = depthKey,
                                    .stableKey   = stableKey,
                                    .drawLast    = false,
                                });
                            };

                            for (int ofs = 0; ofs < count; ofs += 3)
                            {
                                const int base = first + ofs;
                                const TriVertex& v0 = spriteTriVertices_[static_cast<std::size_t>(base + 0)];
                                const TriVertex& v1 = spriteTriVertices_[static_cast<std::size_t>(base + 1)];
                                const TriVertex& v2 = spriteTriVertices_[static_cast<std::size_t>(base + 2)];

                                const int pid = static_cast<int>(v0.pickId + 0.5f);
                                if (pid != runPid)
                                {
                                    flushRun(base);
                                    runStart = base;
                                    runPid = pid;
                                    runPreviewed = isPreviewedPid(runPid);
                                    runBoundsMin = QVector3D(
                                        std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max());
                                    runBoundsMax = QVector3D(
                                        -std::numeric_limits<float>::max(),
                                        -std::numeric_limits<float>::max(),
                                        -std::numeric_limits<float>::max());
                                    runHasBounds = false;
                                }

                                QVector3D p0(v0.x, v0.y, v0.z);
                                QVector3D p1(v1.x, v1.y, v1.z);
                                QVector3D p2(v2.x, v2.y, v2.z);
                                if (runPreviewed)
                                {
                                    p0 = transformPreviewMatrix_.map(p0);
                                    p1 = transformPreviewMatrix_.map(p1);
                                    p2 = transformPreviewMatrix_.map(p2);
                                }

                                const auto expandRunBounds = [&](const QVector3D& p)
                                {
                                    runBoundsMin.setX(std::min(runBoundsMin.x(), p.x()));
                                    runBoundsMin.setY(std::min(runBoundsMin.y(), p.y()));
                                    runBoundsMin.setZ(std::min(runBoundsMin.z(), p.z()));
                                    runBoundsMax.setX(std::max(runBoundsMax.x(), p.x()));
                                    runBoundsMax.setY(std::max(runBoundsMax.y(), p.y()));
                                    runBoundsMax.setZ(std::max(runBoundsMax.z(), p.z()));
                                    runHasBounds = true;
                                };
                                expandRunBounds(p0);
                                expandRunBounds(p1);
                                expandRunBounds(p2);
                            }

                            flushRun(first + count);
                            continue;
                        }

                        const bool drawLast =
                            (batch.kind == TriKind::Level)
                            && ((batch.faceFlags & FF_RD_Adjoin) != 0);
                        items.push_back(DrawItem{
                            .batch       = &batch,
                            .firstVertex = batch.firstVertex,
                            .vertexCount = batch.vertexCount,
                            .depthKey    = batchDepthKey(batch),
                            .stableKey   = batch.firstVertex,
                            .drawLast    = drawLast,
                        });
                    }

                    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b)
                        {
                            if (a.drawLast != b.drawLast)
                            {
                                return a.drawLast < b.drawLast;
                            }
                            if (a.depthKey != b.depthKey)
                            {
                                return a.depthKey > b.depthKey;
                            }
                            if (a.stableKey != b.stableKey)
                            {
                                return a.stableKey < b.stableKey;
                            }
                            return a.firstVertex < b.firstVertex;
                        });

                    for (const DrawItem& item : items)
                    {
                        if (item.batch)
                        {
                            if (item.firstVertex == item.batch->firstVertex && item.vertexCount == item.batch->vertexCount)
                            {
                                drawOneBatch(*item.batch);
                            }
                            else
                            {
                                drawBatchRange(*item.batch, item.firstVertex, item.vertexCount, false);
                            }
                        }
                    }
                }
            }

            if (!lastCullEnabled)
            {
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
            }

            if (std::abs(lastNdcZBias) > 1.0e-9f)
            {
                program_->setUniformValue("u_ndc_z_bias", 0.0f);
            }

            if (!lastPolyOffsetEnabled)
            {
                f->glEnable(GL_POLYGON_OFFSET_FILL);
            }

            const bool wantDepthWrite = !translucentPass;
            if (lastDepthWrite != wantDepthWrite)
            {
                f->glDepthMask(wantDepthWrite ? GL_TRUE : GL_FALSE);
            }
            program_->setUniformValue("u_fog_enabled", false);
        };

        auto drawSkyBatches = [&]()
        {
            if (!projectedSky)
            {
                return;
            }

            for (const TriBatch& batch : triBatches_)
            {
                if (batch.kind != TriKind::Level || batch.skyGroup == 0)
                {
                    continue;
                }

                bool textured = false;
                bool alphaTest = false;
                QVector2D uvScale(1.0f, 1.0f);
                QVector2D uvOffset(0.0f, 0.0f);

                if (showTextures_)
                {
                    if (const auto it = textures_.find(batch.key); it != textures_.end() && it->second.texture)
                    {
                        const TextureInfo& info = it->second;
                        textured = true;
                        if ((batch.skyGroup == 1 || batch.uvNormalized) && info.width > 0 && info.height > 0)
                        {
                            uvScale.setX(1.0f / static_cast<float>(info.width));
                            uvScale.setY(1.0f / static_cast<float>(info.height));
                        }
                        const bool alphaRef = (batch.faceFlags & FF_RD_AlphaRef) != 0u;
                        alphaTest = (info.alphaBpp == 1) || alphaRef;
                        info.texture->bind(0);
                    }
                }

                program_->setUniformValue("u_textured", textured ? 1 : 0);
                program_->setUniformValue("u_alpha_test", alphaTest ? 1 : 0);
                constexpr float kAlphaCutoutRef = 0.5f;
                program_->setUniformValue("u_alpha_ref", alphaTest ? kAlphaCutoutRef : 0.01f);
                program_->setUniformValue("u_uv_scale", uvScale);
                program_->setUniformValue("u_uv_offset", uvOffset);
                program_->setUniformValue("u_flat_shade", false);
                f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
            }
        };

        f->glDisable(GL_BLEND);
        program_->setUniformValue(
            "u_apply_classic_sector_tint",
            level_
                && render::classicBatchUsesCameraSectorTint(
                    level_->kind,
                    renderPipeline_ == RenderPipeline::GamePreview,
                    true,
                    0u));
        program_->setUniformValue("u_apply_local_classic_sector_tint", false);

        // Draw sky first so it fills the background behind world/thing geometry.
        // Use z=1.0 + LEQUAL so it passes against the cleared depth buffer, and do not write depth so it
        // never interferes with far geometry precision.
        if (projectedSky)
        {
            f->glEnable(GL_DEPTH_TEST);
            f->glDepthFunc(GL_LEQUAL);
            f->glDepthMask(GL_FALSE);

            program_->setUniformValue("u_flat_shade", false);
            triVao_->bind();
            drawSkyBatches();
            triVao_->release();

            // Use LEQUAL for the main pass so far-plane geometry (z == 1.0) is still rendered on top of the sky.
            f->glDepthFunc(GL_LEQUAL);
            f->glDepthMask(GL_TRUE);
        }

        const bool fillOwnedWireOverlayEnabled =
            renderPipeline_ == RenderPipeline::Editor
            && canDrawLevelSolid
            && canDrawLevelLines
            && showWireframe_;
        const bool useLayerWireColorTexture = fillOwnedWireOverlayEnabled
            && layerWireframeColoringEnabled_
            && !layerColors_.empty();
        int wireColorCount = 0;
        bool wireColorTextureBound = false;
        if (useLayerWireColorTexture)
        {
            const int requestedColorCount = static_cast<int>(std::min<std::size_t>(
                layerColors_.size(),
                static_cast<std::size_t>(std::max(1, surfaceWireColorMaxTextureSize_))));
            if (!surfaceWireColorTexture_
                || surfaceWireColorTextureLayerCount_ != requestedColorCount)
            {
                surfaceWireColorTexture_.reset();
                surfaceWireColorTexture_ = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
                surfaceWireColorTexture_->setSize(requestedColorCount, 1);
                surfaceWireColorTexture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
                surfaceWireColorTexture_->setMipLevels(1);
                surfaceWireColorTexture_->allocateStorage(
                    QOpenGLTexture::RGBA,
                    QOpenGLTexture::UInt8);
                surfaceWireColorTexture_->setMinificationFilter(QOpenGLTexture::Nearest);
                surfaceWireColorTexture_->setMagnificationFilter(QOpenGLTexture::Nearest);
                surfaceWireColorTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
                surfaceWireColorTextureLayerCount_ = requestedColorCount;
                surfaceWireColorTextureDirty_ = true;
            }

            if (surfaceWireColorTexture_
                && surfaceWireColorTexture_->isCreated()
                && surfaceWireColorTexture_->isStorageAllocated())
            {
                if (surfaceWireColorTextureDirty_)
                {
                    std::vector<unsigned char> colorBytes(
                        static_cast<std::size_t>(requestedColorCount) * 4u,
                        0u);
                    const QColor fallback = wireframeColor_;
                    for (int i = 0; i < requestedColorCount; ++i)
                    {
                        const QColor color = layerColors_[static_cast<std::size_t>(i)];
                        const std::size_t offset = static_cast<std::size_t>(i) * 4u;
                        if (color.isValid())
                        {
                            colorBytes[offset + 0u] = static_cast<unsigned char>(color.red());
                            colorBytes[offset + 1u] = static_cast<unsigned char>(color.green());
                            colorBytes[offset + 2u] = static_cast<unsigned char>(color.blue());
                            colorBytes[offset + 3u] = 255u;
                        }
                        else
                        {
                            colorBytes[offset + 0u] = static_cast<unsigned char>(fallback.red());
                            colorBytes[offset + 1u] = static_cast<unsigned char>(fallback.green());
                            colorBytes[offset + 2u] = static_cast<unsigned char>(fallback.blue());
                            colorBytes[offset + 3u] = 0u;
                        }
                    }
                    surfaceWireColorTexture_->setData(
                        QOpenGLTexture::RGBA,
                        QOpenGLTexture::UInt8,
                        colorBytes.data());
                    surfaceWireColorTextureDirty_ = false;
                }
                surfaceWireColorTexture_->bind(1);
                wireColorCount = requestedColorCount;
                wireColorTextureBound = true;
            }
        }

        const QVector4D fillWireColor(
            wireframeColor_.redF(),
            wireframeColor_.greenF(),
            wireframeColor_.blueF(),
            1.0f);
        program_->setUniformValue("u_surface_wire_overlay_enabled", fillOwnedWireOverlayEnabled);
        const render::WireframeAaPolicy surfaceWireAa = render::wireframeAaPolicy(
            wireframeAntialiasingEnabled_,
            thickLineProgram_ != nullptr);
        program_->setUniformValue(
            "u_surface_wire_antialiasing_enabled",
            surfaceWireAa.surfaceOwnedEdgeSmoothing);
        program_->setUniformValue(
            "u_surface_wire_layer_colors_enabled",
            fillOwnedWireOverlayEnabled && layerWireframeColoringEnabled_);
        program_->setUniformValue("u_surface_wire_width_px", std::max(0.5f, wireframeLineWidthPx_));
        program_->setUniformValue("u_surface_wire_color", fillWireColor);
        program_->setUniformValue("u_surface_wire_color_texture", 1);
        program_->setUniformValue("u_surface_wire_color_count", wireColorCount);

        triVao_->bind();
        drawTriBatches(triBatches_, false);
        triVao_->release();

        if (spriteTriVao_ && spriteTriVertexCount_ > 0)
        {
            spriteTriVao_->bind();
            drawTriBatches(spriteTriBatches_, false);
            spriteTriVao_->release();
        }
        program_->setUniformValue("u_surface_wire_overlay_enabled", false);
        if (wireColorTextureBound)
        {
            surfaceWireColorTexture_->release(1);
            f->glActiveTexture(GL_TEXTURE0);
        }

        // Alpha-depth prepass for eligible translucent scene geometry:
        // Near-opaque texels in a blended wall must still occlude translucent
        // Things behind it even though the normal blend pass does not write
        // depth; transparent vines/cutouts remain visible through. The same
        // final-alpha test stabilizes overlapping 3DO faces without full OIT.
        const bool didAlphaDepthPrepass = [&]() -> bool
        {
            if (!showTextures_ || (!canDrawThingSolid && !canDrawLevelSolid) || !triVao_)
            {
                return false;
            }

            constexpr float kAlphaDepthRef = 0.95f;

            bool did = false;

            f->glDisable(GL_BLEND);
            f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            f->glDepthMask(GL_TRUE);
            f->glDepthFunc(GL_LESS);
            f->glEnable(GL_CULL_FACE);
            f->glCullFace(GL_BACK);
            program_->setUniformValue("u_fog_enabled", false);
            program_->setUniformValue("u_alpha_test_final", 1);

            bool lastCullEnabled = true;

            triVao_->bind();

            for (const TriBatch& batch : triBatches_)
            {
                if ((batch.kind != TriKind::Thing && batch.kind != TriKind::Level)
                    || batch.vertexCount <= 0
                    || (batch.kind == TriKind::Thing && !canDrawThingSolid)
                    || (batch.kind == TriKind::Level && (!canDrawLevelSolid || batch.skyGroup != 0)))
                {
                    continue;
                }

                if (batch.firstVertex < 0)
                {
                    continue;
                }

                const auto itTex = textures_.find(batch.key);
                if (itTex == textures_.end() || !itTex->second.texture)
                {
                    continue;
                }

                const TextureInfo& info = itTex->second;
                const bool iconCube =
                    (batch.kind == TriKind::Thing)
                    && std::string_view(batch.material).rfind(kThingIconMatPrefix, 0) == 0;

                bool translucent = (batch.faceFlags & (FF_Transluent | FF_ZWriteDisabled)) != 0;
                if (batch.kind == TriKind::Thing)
                {
                    translucent = translucent || (batch.minAlpha < 0.999f);
                    if (!translucent && !iconCube && info.alphaBpp > 1)
                    {
                        translucent = true;
                    }
                }
                const render::translucency::PrimitiveKind primitiveKind =
                    batch.kind == TriKind::Level
                        ? render::translucency::PrimitiveKind::level
                        : render::translucency::PrimitiveKind::thing;
                if (batch.isSprite
                    || !render::translucency::allowsAlphaDepthPrepassForBatch(
                        primitiveKind,
                        translucent,
                        info.alphaBpp > 0,
                        (batch.faceFlags & FF_ZWriteDisabled) != 0,
                        batch.skyGroup != 0,
                        (batch.faceFlags & FF_RD_Adjoin) != 0))
                {
                    continue;
                }

                QVector2D uvScale(1.0f, 1.0f);
                QVector2D uvOffset(0.0f, 0.0f);

                if (batch.uvNormalized)
                {
                    uvScale = QVector2D(1.0f, 1.0f);
                }
                else if (info.width > 0 && info.height > 0)
                {
                    uvScale = QVector2D(1.0f / static_cast<float>(info.width), 1.0f / static_cast<float>(info.height));
                }

                const bool wantCull =
                    (batch.kind != TriKind::Thing) || !batch.doubleSided;
                if (wantCull != lastCullEnabled)
                {
                    if (wantCull)
                    {
                        f->glEnable(GL_CULL_FACE);
                        f->glCullFace(GL_BACK);
                    }
                    else
                    {
                        f->glDisable(GL_CULL_FACE);
                    }
                    lastCullEnabled = wantCull;
                }

                program_->setUniformValue("u_textured", 1);
                program_->setUniformValue("u_alpha_test", 1);
                program_->setUniformValue("u_alpha_ref", kAlphaDepthRef);
                program_->setUniformValue("u_uv_scale", uvScale);
                program_->setUniformValue("u_uv_offset", uvOffset);
                program_->setUniformValue("u_flat_shade", false);

                info.texture->bind(0);

                f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
                did = true;
            }

            triVao_->release();

            if (!lastCullEnabled)
            {
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
            }

            // Restore expected state for the main translucent pass.
            f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            program_->setUniformValue("u_alpha_test", 0);
            program_->setUniformValue("u_alpha_test_final", 0);
            program_->setUniformValue("u_alpha_ref", 0.01f);

            return did;
        }();

        // A successful prepass needs LEQUAL so its own color fragments can
        // match the depth it just wrote. With no prepass, restore the scene
        // function that was active before admission (projected sky keeps the
        // main scene on LEQUAL for valid far-plane geometry).
        const GLenum sceneDepthFunc = projectedSky ? GL_LEQUAL : GL_LESS;
        f->glDepthFunc(didAlphaDepthPrepass ? GL_LEQUAL : sceneDepthFunc);

        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        f->glDepthMask(GL_FALSE);
        triVao_->bind();
        drawTriBatches(triBatches_, true);
        triVao_->release();

        if (!drewSortedSpriteTranslucency && spriteTriVao_ && spriteTriVertexCount_ > 0)
        {
            spriteTriVao_->bind();
            drawTriBatches(spriteTriBatches_, true);
            spriteTriVao_->release();
        }

        f->glDepthFunc(sceneDepthFunc);

        f->glDepthMask(GL_TRUE);
        f->glDisable(GL_BLEND);

        f->glDisable(GL_POLYGON_OFFSET_FILL);
        f->glDisable(GL_CULL_FACE);
        // Selection outlines, points, grids, and editor gizmos are UI overlays,
        // not classic scene primitives affected by the game's screen tint.
        program_->setUniformValue("u_apply_classic_sector_tint", false);
        program_->setUniformValue("u_apply_local_classic_sector_tint", false);

        const bool haveCopiedThingOutline = outlineTriVao_ && outlineTriVertexCount_ > 0;
        const bool haveSourceThingOutline = !outlineSourceRanges_.empty();
        if (canDrawThingSolid && (haveCopiedThingOutline || haveSourceThingOutline) && outlinedThing_ >= 0 && level_
            && static_cast<std::size_t>(outlinedThing_) < level_->things.size())
        {
            constexpr float kOutlinePx = 3.0f;
            const QVector3D outlineCenter = outlinedThingCenter_;
            constexpr std::array<QVector2D, 16> kOutlineDirs = {
                QVector2D(1.0f, 0.0f),
                QVector2D(-1.0f, 0.0f),
                QVector2D(0.0f, 1.0f),
                QVector2D(0.0f, -1.0f),
                QVector2D(1.0f, 1.0f),
                QVector2D(-1.0f, 1.0f),
                QVector2D(1.0f, -1.0f),
                QVector2D(-1.0f, -1.0f),
                QVector2D(2.0f, 1.0f),
                QVector2D(-2.0f, 1.0f),
                QVector2D(2.0f, -1.0f),
                QVector2D(-2.0f, -1.0f),
                QVector2D(1.0f, 2.0f),
                QVector2D(-1.0f, 2.0f),
                QVector2D(1.0f, -2.0f),
                QVector2D(-1.0f, -2.0f),
            };
            const int outlineMultiCount = std::clamp(outlineTriMultiVertexCount_, 0, outlineTriVertexCount_);
            const int outlinePrimaryFirst = outlineMultiCount;
            const int outlinePrimaryCount = std::max(0, outlineTriVertexCount_ - outlinePrimaryFirst);

            auto drawSourceOutlineRanges = [&](const std::optional<bool> primaryFilter) -> void
            {
                auto drawSource = [&](const OutlineSource source, QOpenGLVertexArrayObject* vao) -> void
                {
                    if (!vao)
                    {
                        return;
                    }
                    bool bound = false;
                    for (const OutlineSourceRange& range : outlineSourceRanges_)
                    {
                        if (range.source != source || range.count <= 0
                            || (primaryFilter && range.primary != *primaryFilter))
                        {
                            continue;
                        }
                        if (!bound)
                        {
                            vao->bind();
                            bound = true;
                        }
                        f->glDrawArrays(GL_TRIANGLES, range.first, range.count);
                    }
                    if (bound)
                    {
                        vao->release();
                    }
                };

                drawSource(OutlineSource::ThingTriangles, triVao_.get());
                drawSource(OutlineSource::SpriteTriangles, spriteTriVao_.get());
            };

            auto drawOffsetOutlines = [&](const bool primary, const QVector4D& color) -> void
            {
                const int copiedFirst = primary ? outlinePrimaryFirst : 0;
                const int copiedCount = primary ? outlinePrimaryCount : outlineMultiCount;

                program_->setUniformValue("u_color", color);
                for (const QVector2D& dir : kOutlineDirs)
                {
                    program_->setUniformValue("u_outline_dir", dir);
                    if (copiedCount > 0 && outlineTriVao_)
                    {
                        outlineTriVao_->bind();
                        f->glDrawArrays(GL_TRIANGLES, copiedFirst, copiedCount);
                        outlineTriVao_->release();
                    }
                    drawSourceOutlineRanges(primary);
                }
            };

            f->glEnable(GL_STENCIL_TEST);
            f->glClear(GL_STENCIL_BUFFER_BIT);

            f->glStencilMask(0xFF);
            f->glStencilFunc(GL_ALWAYS, 1, 0xFF);
            f->glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            f->glDepthMask(GL_FALSE);
            f->glDisable(GL_DEPTH_TEST);

            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue("u_geo_min", 0);
            program_->setUniformValue("u_outline_center", outlineCenter);
            program_->setUniformValue("u_outline_scale", 1.0f);
            program_->setUniformValue("u_outline_px", 0.0f);
            program_->setUniformValue("u_outline_dir", QVector2D(1.0f, 0.0f));
            program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
            program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));
            program_->setUniformValue("u_textured", 0);
            program_->setUniformValue("u_alpha_test", 0);
            program_->setUniformValue("u_alpha_test_final", 0);
            program_->setUniformValue("u_force_opaque_alpha", true);
            program_->setUniformValue("u_flat_shade", false);

            if (haveCopiedThingOutline)
            {
                outlineTriVao_->bind();
                f->glDrawArrays(GL_TRIANGLES, 0, outlineTriVertexCount_);
                outlineTriVao_->release();
            }
            drawSourceOutlineRanges(std::nullopt);

            f->glEnable(GL_DEPTH_TEST);
            f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            f->glStencilMask(0x00);
            f->glStencilFunc(GL_NOTEQUAL, 1, 0xFF);

            // Disable face culling for the outline pass so single-sided meshes (e.g. planes)
            // still show an outline regardless of which side is facing the camera.
            f->glDisable(GL_CULL_FACE);

            f->glEnable(GL_BLEND);
            f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Visible outline.
            f->glDepthFunc(GL_LEQUAL);
            program_->setUniformValue("u_outline_scale", 1.0f);
            program_->setUniformValue("u_outline_px", kOutlinePx);
            drawOffsetOutlines(
                false,
                QVector4D(
                    effectiveSelectionMultiColor.redF(),
                    effectiveSelectionMultiColor.greenF(),
                    effectiveSelectionMultiColor.blueF(),
                    1.0f));
            drawOffsetOutlines(
                true,
                QVector4D(
                    effectiveSelectionPrimaryColor.redF(),
                    effectiveSelectionPrimaryColor.greenF(),
                    effectiveSelectionPrimaryColor.blueF(),
                    1.0f));

            // Occluded outline (through walls).
            f->glDepthFunc(GL_GREATER);
            drawOffsetOutlines(
                false,
                QVector4D(
                    effectiveSelectionMultiColor.redF(),
                    effectiveSelectionMultiColor.greenF(),
                    effectiveSelectionMultiColor.blueF(),
                    0.55f));
            drawOffsetOutlines(
                true,
                QVector4D(
                    effectiveSelectionPrimaryColor.redF(),
                    effectiveSelectionPrimaryColor.greenF(),
                    effectiveSelectionPrimaryColor.blueF(),
                    0.55f));
            f->glEnable(GL_CULL_FACE);
            f->glCullFace(GL_BACK);
            f->glDisable(GL_STENCIL_TEST);
            f->glStencilMask(0xFF);
            f->glDepthMask(GL_TRUE);
            f->glDisable(GL_BLEND);
            f->glDepthFunc(GL_LESS);

            program_->setUniformValue("u_outline_center", QVector3D(0.0f, 0.0f, 0.0f));
            program_->setUniformValue("u_outline_scale", 1.0f);
            program_->setUniformValue("u_outline_px", 0.0f);
            program_->setUniformValue("u_outline_dir", QVector2D(1.0f, 0.0f));
            program_->setUniformValue("u_force_opaque_alpha", false);
        }
    }

    if (canDrawGrid && !gridDrawPolicy.drawBeforeGeometry)
    {
        // Late grid modes either respect the completed scene depth (default)
        // or deliberately overlay it (Visible Through Walls).
        drawGridPass(gridDrawPolicy.depthTestScene);
    }

    const bool hasOverlayPrimitives = canDrawPoints || canDrawAnyLines;
    const bool overlayDepthTestWasEnabled = f->glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    GLint overlayPreviousDepthFunc = GL_LESS;
    GLboolean overlayPreviousDepthMask = GL_TRUE;
    f->glGetIntegerv(GL_DEPTH_FUNC, &overlayPreviousDepthFunc);
    f->glGetBooleanv(GL_DEPTH_WRITEMASK, &overlayPreviousDepthMask);
    if (hasOverlayPrimitives)
    {
        f->glDepthFunc(GL_LEQUAL);
        f->glDepthMask(GL_FALSE);
    }

    const render::LevelGeometryVertexMarkerSizes levelMarkerSizes =
        render::levelGeometryVertexMarkerSizes(
            levelGeometryVertexMarkerSizePx_, forceLevelPoints);

    if (canDrawPoints)
    {
        // Keep regular point markers depth-tested.
        // Selected vertices are drawn in a second pass on top (below).
        const bool vertexModeOccludes =
            renderPipeline_ == RenderPipeline::Editor
            && surfaceSubMode_ == SurfaceSubMode::Vertex;
        program_->setUniformValue("u_ndc_z_bias", vertexModeOccludes ? 1.0e-5f : 0.0f);

        const QVector4D wireColor(
            wireframeColor_.redF(),
            wireframeColor_.greenF(),
            wireframeColor_.blueF(),
            1.0f);

         program_->setUniformValue("u_lighting_mode", 0);
        program_->setUniformValue("u_geo_min", 0);
        program_->setUniformValue("u_color", wireColor);
        program_->setUniformValue("u_textured", 0);

        if (canDrawLevelPoints || canDrawThingPoints)
        {
            pointVao_->bind();
            f->glVertexAttrib1f(3, 0.0f);

            if (canDrawLevelPoints)
            {
                program_->setUniformValue("u_point_size", levelMarkerSizes.level);
                f->glDrawArrays(GL_POINTS, 0, levelPointVertexCount_);
            }

            if (canDrawThingPoints)
            {
                program_->setUniformValue("u_color", QVector4D(0.60f, 0.78f, 1.00f, 1.0f));
                program_->setUniformValue(
                    "u_point_size",
                    thingViewMode_ == ThingViewMode::Vertices ? 4.0f : 7.0f);
                f->glDrawArrays(GL_POINTS, thingPointFirstVertex_, thingPointVertexCount_);
            }

            pointVao_->release();
        }

        if (canDrawHighlightPoints)
        {
            const int total = highlightPointVertexCount_;
            const int multiCount = std::clamp(highlightPointMultiVertexCount_, 0, total);
            const int selectedFirst = std::clamp(highlightPointSelectedFirstPoint_, 0, total);
            const int selectedCount = std::clamp(highlightPointSelectedPointCount_, 0, total - selectedFirst);

            int activeFirst = -1;
            int activeCount = 0;
            const bool activeValid =
                (activeVertexHighlightPointCount_ > 0)
                && (activeVertexHighlightFirstPoint_ >= 0)
                && ((activeVertexHighlightFirstPoint_ + activeVertexHighlightPointCount_) <= total);
            if (activeValid)
            {
                activeFirst = activeVertexHighlightFirstPoint_;
                activeCount = activeVertexHighlightPointCount_;
            }

            const QVector4D multiColor(
                effectiveSelectionMultiColor.redF(),
                effectiveSelectionMultiColor.greenF(),
                effectiveSelectionMultiColor.blueF(),
                1.0f);
            const QVector4D primaryColor(
                effectiveSelectionPrimaryColor.redF(),
                effectiveSelectionPrimaryColor.greenF(),
                effectiveSelectionPrimaryColor.blueF(),
                1.0f);
            const QVector4D selectedVertexColor(
                effectiveSelectionVertexColor.redF(),
                effectiveSelectionVertexColor.greenF(),
                effectiveSelectionVertexColor.blueF(),
                1.0f);
            const QVector4D selectedVertexMultiColor(
                effectiveSelectionVertexMultiColor.redF(),
                effectiveSelectionVertexMultiColor.greenF(),
                effectiveSelectionVertexMultiColor.blueF(),
                1.0f);
            const QVector4D activeColor(
                effectiveSelectionActiveColor.redF(),
                effectiveSelectionActiveColor.greenF(),
                effectiveSelectionActiveColor.blueF(),
                1.0f);

            const float baseSize = levelMarkerSizes.selectedBase;
            const float selectedSize = levelMarkerSizes.selected;
            const float activeSize = levelMarkerSizes.active;

            auto drawPoints = [&](const int first, const int count, const QVector4D& color, const float size) -> void
            {
                if (count <= 0)
                {
                    return;
                }
                program_->setUniformValue("u_color", color);
                program_->setUniformValue("u_point_size", size);
                f->glDrawArrays(GL_POINTS, first, count);
            };

            highlightPointVao_->bind();
            f->glVertexAttrib1f(3, 0.0f);

            // Base points (multi surfaces then primary surface).
            const int baseEnd = selectedFirst;
            if (baseEnd > 0)
            {
                const int multiDraw = std::min(multiCount, baseEnd);
                drawPoints(0, multiDraw, multiColor, baseSize);

                const int primaryFirst = multiDraw;
                const int primaryCount = baseEnd - primaryFirst;
                drawPoints(primaryFirst, primaryCount, primaryColor, baseSize);
            }

            // Selected vertices overlay (larger points).
            const QVector4D selectedOverlayColor =
                (selectedCount + activeCount > 1)
                ? selectedVertexMultiColor
                : selectedVertexColor;
            const bool selectedOnTop = (selectedCount > 0) || (activeCount > 0);
            if (selectedOnTop)
            {
                f->glDisable(GL_DEPTH_TEST);
            }
            drawPoints(selectedFirst, selectedCount, selectedOverlayColor, selectedSize);

            // Active vertex (on top).
            drawPoints(activeFirst, activeCount, activeColor, activeSize);

            if (selectedOnTop)
            {
                f->glEnable(GL_DEPTH_TEST);
            }

            highlightPointVao_->release();
        }

        if (canDrawArchitecturalLightingPoints)
        {
            drawArchitecturalLightingVertices(mvp);
        }

        program_->setUniformValue("u_ndc_z_bias", 0.0f);
    }

    if (canDrawAnyLines)
    {
        const render::WireframeOverlayDepthPolicy wireDepthPolicy =
            render::wireframeOverlayDepthPolicy();
        const render::WireframeOverlayDepthPolicy selectionDepthPolicy =
            render::selectionHighlightOverlayDepthPolicy();
        const render::SelectionGeometryStencilPolicy geometryStencilPolicy =
            render::selectionGeometryStencilPolicy();
        const GLuint geometryStencilBit = static_cast<GLuint>(geometryStencilPolicy.stencilBit);

        // The geometry-shader path expands every GL line into a triangle
        // strip. Its winding depends on endpoint order in screen space, so it
        // must not inherit face culling from a preceding thing-outline or grid
        // pass. Snapshot every fixed-function state touched here and restore
        // it after the overlay; later passes then see exactly the state that
        // was active on entry.
        const bool lineBlendWasEnabled = f->glIsEnabled(GL_BLEND) == GL_TRUE;
        const bool lineMultisampleWasEnabled = f->glIsEnabled(GL_MULTISAMPLE) == GL_TRUE;
        const bool lineSmoothWasEnabled = f->glIsEnabled(GL_LINE_SMOOTH) == GL_TRUE;
        const bool lineCullWasEnabled = f->glIsEnabled(GL_CULL_FACE) == GL_TRUE;
        const bool lineDepthTestWasEnabled = f->glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
        const bool lineStencilWasEnabled = f->glIsEnabled(GL_STENCIL_TEST) == GL_TRUE;
        const bool linePolygonPointWasEnabled = f->glIsEnabled(GL_POLYGON_OFFSET_POINT) == GL_TRUE;
        const bool linePolygonFillWasEnabled = f->glIsEnabled(GL_POLYGON_OFFSET_FILL) == GL_TRUE;
        const bool linePolygonLineWasEnabled = f->glIsEnabled(GL_POLYGON_OFFSET_LINE) == GL_TRUE;
        GLint linePreviousBlendSrcRgb = GL_ONE;
        GLint linePreviousBlendDstRgb = GL_ZERO;
        GLint linePreviousBlendSrcAlpha = GL_ONE;
        GLint linePreviousBlendDstAlpha = GL_ZERO;
        GLint linePreviousLineSmoothHint = GL_DONT_CARE;
        GLint linePreviousCullMode = GL_BACK;
        GLint linePreviousFrontFace = GL_CCW;
        GLint linePreviousDepthFunc = GL_LESS;
        GLint linePreviousStencilFunc = GL_ALWAYS;
        GLint linePreviousStencilRef = 0;
        GLint linePreviousStencilValueMask = 0xFF;
        GLint linePreviousStencilWriteMask = 0xFF;
        GLint linePreviousStencilClearValue = 0;
        GLint linePreviousStencilFail = GL_KEEP;
        GLint linePreviousStencilDepthFail = GL_KEEP;
        GLint linePreviousStencilDepthPass = GL_KEEP;
        GLboolean linePreviousDepthMask = GL_TRUE;
        GLboolean linePreviousColorMask[4]{ GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        GLfloat linePreviousWidth = 1.0f;
        GLfloat linePreviousPolygonOffsetFactor = 0.0f;
        GLfloat linePreviousPolygonOffsetUnits = 0.0f;
        f->glGetIntegerv(GL_BLEND_SRC_RGB, &linePreviousBlendSrcRgb);
        f->glGetIntegerv(GL_BLEND_DST_RGB, &linePreviousBlendDstRgb);
        f->glGetIntegerv(GL_BLEND_SRC_ALPHA, &linePreviousBlendSrcAlpha);
        f->glGetIntegerv(GL_BLEND_DST_ALPHA, &linePreviousBlendDstAlpha);
        f->glGetIntegerv(GL_LINE_SMOOTH_HINT, &linePreviousLineSmoothHint);
        f->glGetIntegerv(GL_CULL_FACE_MODE, &linePreviousCullMode);
        f->glGetIntegerv(GL_FRONT_FACE, &linePreviousFrontFace);
        f->glGetIntegerv(GL_DEPTH_FUNC, &linePreviousDepthFunc);
        f->glGetIntegerv(GL_STENCIL_FUNC, &linePreviousStencilFunc);
        f->glGetIntegerv(GL_STENCIL_REF, &linePreviousStencilRef);
        f->glGetIntegerv(GL_STENCIL_VALUE_MASK, &linePreviousStencilValueMask);
        f->glGetIntegerv(GL_STENCIL_WRITEMASK, &linePreviousStencilWriteMask);
        f->glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &linePreviousStencilClearValue);
        f->glGetIntegerv(GL_STENCIL_FAIL, &linePreviousStencilFail);
        f->glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &linePreviousStencilDepthFail);
        f->glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &linePreviousStencilDepthPass);
        f->glGetBooleanv(GL_DEPTH_WRITEMASK, &linePreviousDepthMask);
        f->glGetBooleanv(GL_COLOR_WRITEMASK, linePreviousColorMask);
        f->glGetFloatv(GL_LINE_WIDTH, &linePreviousWidth);
        f->glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &linePreviousPolygonOffsetFactor);
        f->glGetFloatv(GL_POLYGON_OFFSET_UNITS, &linePreviousPolygonOffsetUnits);

        if (wireDepthPolicy.cullScreenSpaceQuads)
        {
            f->glEnable(GL_CULL_FACE);
        }
        else
        {
            f->glDisable(GL_CULL_FACE);
        }
        if (wireDepthPolicy.depthTestScene)
        {
            // Keep completed scene depth authoritative. Supporting-plane
            // matching resolves coplanar thick-line coverage without pulling
            // the overlay toward the camera.
            f->glEnable(GL_DEPTH_TEST);
        }
        else
        {
            f->glDisable(GL_DEPTH_TEST);
        }
        if (!wireDepthPolicy.polygonOffset)
        {
            f->glDisable(GL_POLYGON_OFFSET_FILL);
            f->glDisable(GL_POLYGON_OFFSET_LINE);
        }
        const render::WireframeAaPolicy lineAa = render::wireframeAaPolicy(
            wireframeAntialiasingEnabled_,
            thickLineProgram_ != nullptr);
        if (lineAa.multisample)
        {
            f->glEnable(GL_MULTISAMPLE);
        }
        else
        {
            f->glDisable(GL_MULTISAMPLE);
        }
        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        f->glDepthMask(GL_FALSE);
        f->glDepthFunc(GL_LEQUAL);
        const QVector4D lineColor(
            wireframeColor_.redF(),
            wireframeColor_.greenF(),
            wireframeColor_.blueF(),
            1.0f);
        auto layerColorVector = [this, lineColor](const std::int32_t layer) -> QVector4D
        {
            if (layer >= 0 && static_cast<std::size_t>(layer) < layerColors_.size())
            {
                QColor c = layerColors_[static_cast<std::size_t>(layer)];
                if (c.isValid())
                {
                    c.setAlpha(255);
                    return QVector4D(c.redF(), c.greenF(), c.blueF(), 1.0f);
                }
            }
            return lineColor;
        };

        const QVector4D thingCollisionOverlayColor(0.12f, 0.98f, 0.28f, 1.0f);
        const QVector4D thingBroadPhaseTreeGuideColor(0.92f, 0.34f, 1.0f, 1.0f);
        const QVector4D thingVisualBoundsOverlayColor(1.0f, 0.62f, 0.12f, 1.0f);
        const QVector4D selectedOverlayOutlineColor(0.015f, 0.02f, 0.035f, 1.0f);
        const QVector4D selectedCollisionOverlayColor(0.66f, 1.0f, 0.72f, 1.0f);
        const QVector4D selectedBroadPhaseTreeGuideColor(1.0f, 0.78f, 1.0f, 1.0f);
        const QVector4D selectedVisualBoundsOverlayColor(1.0f, 0.94f, 0.5f, 1.0f);
        const QVector4D templateCollisionBoundsGuideColor(0.22f, 0.82f, 1.0f, 1.0f);
        const std::size_t tailFirst = thingOverlayLineFirstVertex_ >= 0
            ? static_cast<std::size_t>(thingOverlayLineFirstVertex_)
            : std::numeric_limits<std::size_t>::max();
        const std::size_t tailCount = thingOverlayLineVertexCount_ >= 0
            ? static_cast<std::size_t>(thingOverlayLineVertexCount_)
            : 0u;
        const std::size_t availableLineVertices = lineVertices_.size();
        const bool overlayTailValid = tailFirst <= availableLineVertices
            && tailCount <= availableLineVertices - tailFirst;
        const std::size_t tailEnd = overlayTailValid ? tailFirst + tailCount : tailFirst;
        // Keep stable per-class draw parameters beside the composed mesh. A
        // repaint of a large level then submits one multi-draw per class rather
        // than walking and submitting one GL call for every owner range.
        const auto cacheThingOverlayDrawCommands =
            [this, tailFirst, tailEnd, overlayTailValid, availableLineVertices](
                const std::vector<render::DrawRange>& ranges,
                ThingOverlayDrawCommands& commands)
        {
            commands.firsts.clear();
            commands.counts.clear();
            if (!overlayTailValid)
            {
                return;
            }
            commands.firsts.reserve(ranges.size());
            commands.counts.reserve(ranges.size());
            for (const render::DrawRange& range : ranges)
            {
                if (!render::isValidDrawRange(range, availableLineVertices))
                {
                    continue;
                }
                const std::size_t rangeFirst = static_cast<std::size_t>(range.first);
                const std::size_t rangeCount = static_cast<std::size_t>(range.count);
                if (rangeFirst < tailFirst || rangeFirst > tailEnd
                    || rangeCount > (tailEnd - rangeFirst))
                {
                    continue;
                }
                commands.firsts.push_back(static_cast<GLint>(range.first));
                commands.counts.push_back(static_cast<GLsizei>(range.count));
            }
        };

        if (!thingOverlayDrawCacheMeshRevisionValid_
            || thingOverlayDrawCacheMeshRevision_ != meshRebuildCount_)
        {
            thingOverlayDrawCacheMeshRevision_ = meshRebuildCount_;
            thingOverlayDrawCacheMeshRevisionValid_ = true;
            cacheThingOverlayDrawCommands(
                thingCollisionOverlayLineRanges_, thingCollisionOverlayDrawCommands_);
            cacheThingOverlayDrawCommands(
                thingBroadPhaseTreeGuideLineRanges_, thingBroadPhaseOverlayDrawCommands_);
            cacheThingOverlayDrawCommands(
                thingVisualBoundsOverlayLineRanges_, thingVisualBoundsOverlayDrawCommands_);
            cacheThingOverlayDrawCommands(
                thingTemplateCollisionBoundsGuideLineRanges_, thingTemplateBoundsOverlayDrawCommands_);
            thingOverlayDrawCacheSelectionValid_ = false;
        }

        // Selection changes are usually small; compare only the selected
        // owners each paint and refresh their emphasized draw lists on change.
        const bool selectedOwnerSetMatches = thingOverlayDrawCacheSelectionValid_
            && thingOverlayDrawCacheSelectedThing_ == selectedThing_
            && thingOverlayDrawCacheSelectedOwners_.size() == highlightedSelection_.things.size()
            && std::equal(
                thingOverlayDrawCacheSelectedOwners_.begin(),
                thingOverlayDrawCacheSelectedOwners_.end(),
                highlightedSelection_.things.begin());
        if (!selectedOwnerSetMatches)
        {
            thingOverlayDrawCacheSelectedOwners_.assign(
                highlightedSelection_.things.begin(), highlightedSelection_.things.end());
            thingOverlayDrawCacheSelectedThing_ = selectedThing_;
            thingOverlayDrawCacheSelectionValid_ = true;
            cacheThingOverlayDrawCommands(
                render::thing_overlay::selectedThingOverlayRanges(
                    highlightedSelection_.things,
                    selectedThing_,
                    thingPickIds_,
                    thingCollisionOverlayRangesByPickId_,
                    thingOverlayLineFirstVertex_),
                thingSelectedCollisionDrawCommands_);
            cacheThingOverlayDrawCommands(
                render::thing_overlay::selectedThingOverlayRanges(
                    highlightedSelection_.things,
                    selectedThing_,
                    thingPickIds_,
                    thingBroadPhaseTreeGuideRangesByPickId_,
                    thingOverlayLineFirstVertex_),
                thingSelectedBroadPhaseDrawCommands_);
            cacheThingOverlayDrawCommands(
                render::thing_overlay::selectedThingOverlayRanges(
                    highlightedSelection_.things,
                    selectedThing_,
                    thingPickIds_,
                    thingVisualBoundsOverlayRangesByPickId_,
                    thingOverlayLineFirstVertex_),
                thingSelectedVisualBoundsDrawCommands_);
        }

        const MultiDrawArraysFunction multiDrawArrays =
            reinterpret_cast<MultiDrawArraysFunction>(thingOverlayMultiDrawArraysFunction_);
        const auto drawThingOverlayGroup = [this, f, multiDrawArrays](
            QOpenGLShaderProgram* shader,
            const ThingOverlayDrawCommands& commands,
            const QVector4D& color,
            const float lineWidthPx,
            const bool expandedShaderLines)
        {
            if (!shader || !lineVao_ || commands.firsts.empty()
                || commands.firsts.size() != commands.counts.size()
                || thingOverlayLineFirstVertex_ < 0
                || thingOverlayLineVertexCount_ <= 0)
            {
                return;
            }

            const std::size_t tailFirst =
                static_cast<std::size_t>(thingOverlayLineFirstVertex_);
            const std::size_t tailCount =
                static_cast<std::size_t>(thingOverlayLineVertexCount_);
            const std::size_t availableLineVertices = lineVertices_.size();
            if (tailFirst > availableLineVertices
                || tailCount > (availableLineVertices - tailFirst))
            {
                return;
            }
            shader->setUniformValue("u_color", color);
            shader->setUniformValue("u_ndc_z_bias", 0.0f);
            if (expandedShaderLines)
            {
                shader->setUniformValue("u_line_width_px", lineWidthPx);
            }
            else
            {
                f->glLineWidth(lineWidthPx);
            }

            lineVao_->bind();
            if (multiDrawArrays
                && commands.firsts.size()
                    <= static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
            {
                multiDrawArrays(
                    GL_LINES,
                    commands.firsts.data(),
                    commands.counts.data(),
                    static_cast<GLsizei>(commands.firsts.size()));
            }
            else
            {
                for (std::size_t i = 0; i < commands.firsts.size(); ++i)
                {
                    f->glDrawArrays(GL_LINES, commands.firsts[i], commands.counts[i]);
                }
            }
            lineVao_->release();
        };
        const auto drawThingOverlayPasses = [&, this](
            QOpenGLShaderProgram* shader,
            const bool expandedShaderLines)
        {
            if (!canDrawThingOverlayLines)
            {
                return;
            }

            // These independent overlays use the completed opaque scene depth.
            // Keep them out of the geometry-selection stencil mask so selecting
            // a surface cannot clip collision or visual bounds. Polygon offsets
            // belong to the regular wireframe path and must not pull these
            // independent outlines through nearby opaque geometry.
            const bool polygonOffsetFillWasEnabled =
                f->glIsEnabled(GL_POLYGON_OFFSET_FILL) == GL_TRUE;
            const bool polygonOffsetLineWasEnabled =
                f->glIsEnabled(GL_POLYGON_OFFSET_LINE) == GL_TRUE;
            GLfloat previousPolygonOffsetFactor = 0.0f;
            GLfloat previousPolygonOffsetUnits = 0.0f;
            f->glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &previousPolygonOffsetFactor);
            f->glGetFloatv(GL_POLYGON_OFFSET_UNITS, &previousPolygonOffsetUnits);
            f->glDisable(GL_POLYGON_OFFSET_FILL);
            f->glDisable(GL_POLYGON_OFFSET_LINE);
            f->glEnable(GL_DEPTH_TEST);
            f->glDepthFunc(GL_LEQUAL);
            f->glDepthMask(GL_FALSE);
            f->glDisable(GL_STENCIL_TEST);

            if (showThingVisualBounds_)
            {
                drawThingOverlayGroup(
                    shader,
                    thingVisualBoundsOverlayDrawCommands_,
                    thingVisualBoundsOverlayColor,
                    1.5f,
                    expandedShaderLines);
            }
            if (canDrawTemplateCollisionBoundsGuideLines)
            {
                drawThingOverlayGroup(
                    shader,
                    thingTemplateBoundsOverlayDrawCommands_,
                    templateCollisionBoundsGuideColor,
                    1.5f,
                    expandedShaderLines);
            }
            if (showThingCollisionShapes_)
            {
                // Collision remains the strongest visual cue when its wire
                // shares model edges with the amber bounds. MotS broad-phase
                // treesize uses a separate color so it cannot be mistaken for
                // the per-mesh narrow-phase spheres.
                drawThingOverlayGroup(
                    shader,
                    thingCollisionOverlayDrawCommands_,
                    thingCollisionOverlayColor,
                    2.5f,
                    expandedShaderLines);
                drawThingOverlayGroup(
                    shader,
                    thingBroadPhaseOverlayDrawCommands_,
                    thingBroadPhaseTreeGuideColor,
                    1.75f,
                    expandedShaderLines);
            }

            // Re-draw selected owners after the normal overlays so the selected
            // shape can be traced where Things overlap. The dark, wider stroke
            // is only a color/width emphasis: these lines retain this pass's
            // opaque scene depth test and never become visible through walls.
            constexpr float selectedOutlineWidthPx = 5.0f;
            if (render::thing_overlay::shouldEmphasizeThingOverlay(
                    showThingVisualBounds_,
                    !thingSelectedVisualBoundsDrawCommands_.firsts.empty()))
            {
                drawThingOverlayGroup(
                    shader,
                    thingSelectedVisualBoundsDrawCommands_,
                    selectedOverlayOutlineColor,
                    selectedOutlineWidthPx,
                    expandedShaderLines);
                drawThingOverlayGroup(
                    shader,
                    thingSelectedVisualBoundsDrawCommands_,
                    selectedVisualBoundsOverlayColor,
                    2.5f,
                    expandedShaderLines);
            }
            if (render::thing_overlay::shouldEmphasizeThingOverlay(
                    showThingCollisionShapes_,
                    !thingSelectedCollisionDrawCommands_.firsts.empty()))
            {
                drawThingOverlayGroup(
                    shader,
                    thingSelectedCollisionDrawCommands_,
                    selectedOverlayOutlineColor,
                    selectedOutlineWidthPx,
                    expandedShaderLines);
                drawThingOverlayGroup(
                    shader,
                    thingSelectedCollisionDrawCommands_,
                    selectedCollisionOverlayColor,
                    3.0f,
                    expandedShaderLines);
            }
            if (render::thing_overlay::shouldEmphasizeThingOverlay(
                    showThingCollisionShapes_,
                    !thingSelectedBroadPhaseDrawCommands_.firsts.empty()))
            {
                drawThingOverlayGroup(
                    shader,
                    thingSelectedBroadPhaseDrawCommands_,
                    selectedOverlayOutlineColor,
                    selectedOutlineWidthPx,
                    expandedShaderLines);
                drawThingOverlayGroup(
                    shader,
                    thingSelectedBroadPhaseDrawCommands_,
                    selectedBroadPhaseTreeGuideColor,
                    2.5f,
                    expandedShaderLines);
            }

            f->glPolygonOffset(
                previousPolygonOffsetFactor,
                previousPolygonOffsetUnits);
            if (polygonOffsetFillWasEnabled)
            {
                f->glEnable(GL_POLYGON_OFFSET_FILL);
            }
            else
            {
                f->glDisable(GL_POLYGON_OFFSET_FILL);
            }
            if (polygonOffsetLineWasEnabled)
            {
                f->glEnable(GL_POLYGON_OFFSET_LINE);
            }
            else
            {
                f->glDisable(GL_POLYGON_OFFSET_LINE);
            }
        };

        // Mark the selected Sector/Surface fills in one stencil bit.  The
        // regular line overlay still draws every selected polygon edge, so
        // this mask only supplies the Thing-outline-style outer halo. The
        // recorded line ranges also drive the reduced-alpha occluded pass;
        // internal geometry edges remain visible and are never reduced to a
        // silhouette.
        // Ranges point into the existing level triangle VBO and are rebuilt
        // only when the mesh or selection changes.
        const bool geometrySelectionStencilActive =
            !lineStencilWasEnabled
            && triVao_
            && !geometrySelectionStencilRanges_.empty()
            && geometryStencilPolicy.preserveInteriorWireframe
            && geometryStencilPolicy.preserveStencilClearValue;
        if (geometrySelectionStencilActive)
        {
            f->glDisable(GL_BLEND);
            f->glDisable(GL_CULL_FACE);
            if (geometryStencilPolicy.depthTestScene)
            {
                f->glEnable(GL_DEPTH_TEST);
            }
            else
            {
                f->glDisable(GL_DEPTH_TEST);
            }
            f->glDepthFunc(GL_LEQUAL);
            f->glDepthMask(GL_FALSE);
            f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            // Use only bit zero and clear that bit, leaving any future stencil
            // users' bits untouched.  The ordinary frame path starts with
            // stencil disabled, so this pass can safely own the bit until the
            // line overlay completes.
            f->glStencilMask(geometryStencilBit);
            f->glClearStencil(0);
            f->glClear(GL_STENCIL_BUFFER_BIT);
            f->glEnable(GL_STENCIL_TEST);
            f->glStencilFunc(GL_ALWAYS, 1, geometryStencilBit);
            f->glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue("u_geo_min", 0);
            program_->setUniformValue("u_textured", 0);
            program_->setUniformValue("u_alpha_test", 0);
            program_->setUniformValue("u_alpha_test_final", 0);
            program_->setUniformValue("u_ndc_z_bias", 0.0f);

            triVao_->bind();
            const std::size_t availableLevelTriVertices = std::min(
                static_cast<std::size_t>(std::max(0, levelTriVertexCount_)),
                triVertices_.size());
            for (const render::DrawRange& range : geometrySelectionStencilRanges_)
            {
                if (!render::isValidDrawRange(range, availableLevelTriVertices))
                {
                    continue;
                }
                f->glDrawArrays(GL_TRIANGLES, range.first, range.count);
            }
            triVao_->release();

            // Restore the line pass state while retaining the read-only stencil
            // mask for the subsequent dark halo draw.
            f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            f->glStencilMask(0x00);
            f->glStencilFunc(GL_NOTEQUAL, 1, geometryStencilBit);
            f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            f->glEnable(GL_BLEND);
            f->glDepthMask(GL_FALSE);
            f->glDepthFunc(GL_LEQUAL);
        }

        if (thickLineProgram_)
        {
            // Level edges match their supporting surface's projected depth
            // across the expanded screen-space quad. No additional pull is
            // used: scene-depth rejection must remain authoritative for edges
            // belonging to sectors behind solid walls.
            if (wireDepthPolicy.polygonOffset)
            {
                f->glEnable(GL_POLYGON_OFFSET_FILL);
                f->glPolygonOffset(
                    wireDepthPolicy.polygonOffsetFactor,
                    wireDepthPolicy.polygonOffsetUnits);
            }

            const float vpw = std::max(1.0f, static_cast<float>(camera_.canvasWidth()));
            const float vph = std::max(1.0f, static_cast<float>(camera_.canvasHeight()));

            thickLineProgram_->bind();
            thickLineProgram_->setUniformValue("u_mvp", mvp);
            thickLineProgram_->setUniformValue("u_viewport_size", QVector2D(vpw, vph));
            thickLineProgram_->setUniformValue("u_frame_xform_enabled", frameXform.enabled);
            thickLineProgram_->setUniformValue("u_frame_xform_id", frameXform.pickId);
            thickLineProgram_->setUniformValue("u_frame_xform_matrix", frameXform.matrix);
            thickLineProgram_->setUniformValue("u_xform_enabled", transformPreviewActive_ ? 1 : 0);
            thickLineProgram_->setUniformValue("u_xform_count", transformPreviewActive_ ? transformPreviewIdCount_ : 0);
            if (transformPreviewActive_ && transformPreviewIdCount_ > 0)
            {
                thickLineProgram_->setUniformValueArray("u_xform_ids", transformPreviewIds_.data(), transformPreviewIdCount_);
                thickLineProgram_->setUniformValueArray(
                    "u_xform_color_mul",
                    transformPreviewPayloads_.data(),
                    transformPreviewIdCount_);
            }
            thickLineProgram_->setUniformValue("u_xform_matrix", transformPreviewActive_ ? transformPreviewMatrix_ : QMatrix4x4());
            thickLineProgram_->setUniformValue("u_ndc_z_bias", wireDepthPolicy.ndcBias);
            thickLineProgram_->setUniformValue(
                "u_surface_depth_match",
                wireDepthPolicy.matchSupportingSurfaceDepth ? 1 : 0);
            thickLineProgram_->setUniformValue(
                "u_plane_min_sin_squared",
                wireDepthPolicy.projectedPlaneMinSinSquared);
            thickLineProgram_->setUniformValue(
                "u_clip_min_w",
                wireDepthPolicy.homogeneousClipMinimumW);

            if (canDrawLevelLines || canDrawThingLines)
            {
                if (geometrySelectionStencilActive)
                {
                    f->glDisable(GL_STENCIL_TEST);
                }
                thickLineProgram_->setUniformValue("u_line_width_px", std::max(0.5f, wireframeLineWidthPx_));

                lineVao_->bind();
                const bool useLayerColors = layerWireframeColoringEnabled_ && !baseLineLayerRanges_.empty();
                if (!useLayerColors)
                {
                    thickLineProgram_->setUniformValue("u_color", lineColor);
                    if (canDrawLevelLines)
                    {
                        f->glDrawArrays(GL_LINES, 0, wireLineVertexCount_);
                    }
                    if (canDrawThingLines)
                    {
                        f->glDrawArrays(GL_LINES, thingLineFirstVertex_, thingLineVertexCount_);
                    }
                }
                else
                {
                    const int drawFirst = canDrawLevelLines ? 0 : thingLineFirstVertex_;
                    const int drawEnd = canDrawThingLines
                        ? (thingLineFirstVertex_ + thingLineVertexCount_)
                        : wireLineVertexCount_;

                    for (const LayerLineRange& range : baseLineLayerRanges_)
                    {
                        const int start = std::max(drawFirst, range.first);
                        const int end = std::min(drawEnd, range.first + range.count);
                        const int count = end - start;
                        if (count <= 0)
                        {
                            continue;
                        }

                        thickLineProgram_->setUniformValue("u_color", layerColorVector(range.layer));
                        f->glDrawArrays(GL_LINES, start, count);
                    }
                }
                lineVao_->release();
                if (geometrySelectionStencilActive)
                {
                    f->glEnable(GL_STENCIL_TEST);
                    f->glStencilMask(0x00);
                    f->glStencilFunc(GL_NOTEQUAL, 1, geometryStencilBit);
                    f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                }
            }

            if (canDrawHighlightLines)
            {
                highlightLineVao_->bind();

                const int multiCount = std::clamp(
                    highlightLineMultiVertexCount_, 0, highlightLineVertexCount_);
                const int normalFirst = surfaceNormalHighlightFirstVertex_;
                const int normalEnd = normalFirst >= 0
                    && surfaceNormalHighlightVertexCount_
                        <= (std::numeric_limits<int>::max() - normalFirst)
                    ? normalFirst + surfaceNormalHighlightVertexCount_
                    : std::numeric_limits<int>::max();
                const bool normalsValid = render::isValidSelectionNormalRange(
                    render::DrawRange{
                        normalFirst, surfaceNormalHighlightVertexCount_ },
                    multiCount,
                    highlightLineVertexCount_);

                // Selection is the final line overlay.  Mask the ordinary
                // white wire beneath it with a narrow contrast halo, then
                // draw the authored selection color with a tiny coplanar lift.
                // The visible pass remains scene-depth tested, while a second
                // reduced-alpha geometry-only pass below mirrors Thing
                // outlines for selected edges hidden by foreground geometry.
                thickLineProgram_->setUniformValue("u_ndc_z_bias", selectionDepthPolicy.ndcBias);
                thickLineProgram_->setUniformValue("u_color", QVector4D(0.025f, 0.03f, 0.04f, 1.0f));
                thickLineProgram_->setUniformValue("u_line_width_px", 5.0f);
                // Normal arrows are a separate visual marker. Do not paint
                // them with the dark geometry halo; they receive one
                // dedicated cyan draw below.
                if (!normalsValid)
                {
                    f->glDrawArrays(GL_LINES, 0, highlightLineVertexCount_);
                }
                else
                {
                    if (normalFirst > 0)
                    {
                        f->glDrawArrays(GL_LINES, 0, normalFirst);
                    }
                    if (normalEnd < highlightLineVertexCount_)
                    {
                        f->glDrawArrays(
                            GL_LINES,
                            normalEnd,
                            highlightLineVertexCount_ - normalEnd);
                    }
                }
                if (geometrySelectionStencilActive)
                {
                    // The colored pass must include both sides of every edge;
                    // only the contrast halo is stencil-limited to the
                    // outside of the selected fill.
                    f->glStencilFunc(GL_ALWAYS, 0, geometryStencilBit);
                }

                const QVector4D multiColor(
                    effectiveSelectionMultiColor.redF(),
                    effectiveSelectionMultiColor.greenF(),
                    effectiveSelectionMultiColor.blueF(),
                    1.0f);
                const QVector4D edgeMultiColor(
                    effectiveSelectionEdgeMultiColor.redF(),
                    effectiveSelectionEdgeMultiColor.greenF(),
                    effectiveSelectionEdgeMultiColor.blueF(),
                    1.0f);
                const QVector4D primaryColor(
                    effectiveSelectionPrimaryColor.redF(),
                    effectiveSelectionPrimaryColor.greenF(),
                    effectiveSelectionPrimaryColor.blueF(),
                    1.0f);
                const QVector4D activeColor(
                    effectiveSelectionActiveColor.redF(),
                    effectiveSelectionActiveColor.greenF(),
                    effectiveSelectionActiveColor.blueF(),
                    1.0f);
                // Surface normals are a direction aid, not another active
                // transform axis.  Keep them cyan so they cannot visually
                // merge with the red X move/rotate axis (or the yellow/green
                // primary geometry highlight).
                const QVector4D normalColor(0.12f, 0.92f, 1.0f, 1.0f);

                const bool secondaryEdgesValid =
                    secondaryEdgeHighlightVertexCount_ > 0
                    && secondaryEdgeHighlightFirstVertex_ >= 0
                    && secondaryEdgeHighlightFirstVertex_
                        + secondaryEdgeHighlightVertexCount_ == multiCount;
                const int multiBaseCount = secondaryEdgesValid
                    ? secondaryEdgeHighlightFirstVertex_ : multiCount;
                if (multiBaseCount > 0)
                {
                    thickLineProgram_->setUniformValue("u_color", multiColor);
                    thickLineProgram_->setUniformValue("u_line_width_px", 2.8f);
                    f->glDrawArrays(GL_LINES, 0, multiBaseCount);
                }
                if (secondaryEdgesValid)
                {
                    thickLineProgram_->setUniformValue("u_color", edgeMultiColor);
                    thickLineProgram_->setUniformValue("u_line_width_px", 4.0f);
                    f->glDrawArrays(
                        GL_LINES,
                        secondaryEdgeHighlightFirstVertex_,
                        secondaryEdgeHighlightVertexCount_);
                }

                const int primaryStart = multiCount;
                // The normal marker is appended after all selected geometry;
                // keep it out of the primary/active-color range.
                const int primaryEnd = normalsValid
                    ? normalFirst : highlightLineVertexCount_;

                int activeFirst = -1;
                int activeCount = 0;

                const bool activeValid =
                    (activeEdgeHighlightVertexCount_ > 0)
                    && (activeEdgeHighlightFirstVertex_ >= 0)
                    && ((activeEdgeHighlightFirstVertex_ + activeEdgeHighlightVertexCount_) <= primaryEnd)
                    && (activeEdgeHighlightFirstVertex_ >= primaryStart);

                if (activeValid)
                {
                    activeFirst = activeEdgeHighlightFirstVertex_;
                    activeCount = activeEdgeHighlightVertexCount_;
                }

                if (primaryEnd > primaryStart)
                {
                    if (!activeValid)
                    {
                        thickLineProgram_->setUniformValue("u_color", primaryColor);
                        thickLineProgram_->setUniformValue("u_line_width_px", 3.2f);
                        f->glDrawArrays(GL_LINES, primaryStart, primaryEnd - primaryStart);
                    }
                    else
                    {
                        const int baseCount = activeFirst - primaryStart;
                        if (baseCount > 0)
                        {
                            thickLineProgram_->setUniformValue("u_color", primaryColor);
                            thickLineProgram_->setUniformValue("u_line_width_px", 3.2f);
                            f->glDrawArrays(GL_LINES, primaryStart, baseCount);
                        }

                        if (activeCount > 0)
                        {
                            thickLineProgram_->setUniformValue("u_color", activeColor);
                            thickLineProgram_->setUniformValue("u_line_width_px", 4.4f);
                            f->glDrawArrays(GL_LINES, activeFirst, activeCount);
                        }

                        const int afterStart = activeFirst + activeCount;
                        const int afterCount = primaryEnd - afterStart;
                        if (afterCount > 0)
                        {
                            thickLineProgram_->setUniformValue("u_color", primaryColor);
                            thickLineProgram_->setUniformValue("u_line_width_px", 3.2f);
                            f->glDrawArrays(GL_LINES, afterStart, afterCount);
                        }
                    }
                }

                if (normalsValid)
                {
                    thickLineProgram_->setUniformValue("u_color", normalColor);
                    thickLineProgram_->setUniformValue("u_line_width_px", 3.0f);
                    f->glDrawArrays(
                        GL_LINES,
                        surfaceNormalHighlightFirstVertex_,
                        surfaceNormalHighlightVertexCount_);
                }

                highlightLineVao_->release();

                // Mirror the Thing outline's second depth pass for selected
                // Sector/Surface geometry. Only the recorded geometry ranges
                // are drawn here; selected Thing overlays remain owned by
                // their existing outline path and ordinary wireframe lines
                // retain their normal scene-depth behavior.
                const bool canDrawOccludedGeometryLines =
                    geometryStencilPolicy.drawOccludedWireframe
                    && !geometrySelectionLineRanges_.empty();
                if (canDrawOccludedGeometryLines)
                {
                    if (geometrySelectionStencilActive)
                    {
                        f->glDisable(GL_STENCIL_TEST);
                    }

                    f->glEnable(GL_BLEND);
                    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    f->glDepthMask(GL_FALSE);
                    f->glDepthFunc(GL_GREATER);
                    thickLineProgram_->setUniformValue("u_ndc_z_bias", 0.0f);
                    thickLineProgram_->setUniformValue("u_surface_depth_match", 1);
                    const QVector4D occludedMultiColor(
                        effectiveSelectionMultiColor.redF(),
                        effectiveSelectionMultiColor.greenF(),
                        effectiveSelectionMultiColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedEdgeMultiColor(
                        effectiveSelectionEdgeMultiColor.redF(),
                        effectiveSelectionEdgeMultiColor.greenF(),
                        effectiveSelectionEdgeMultiColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedPrimaryColor(
                        effectiveSelectionPrimaryColor.redF(),
                        effectiveSelectionPrimaryColor.greenF(),
                        effectiveSelectionPrimaryColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedActiveColor(
                        effectiveSelectionActiveColor.redF(),
                        effectiveSelectionActiveColor.greenF(),
                        effectiveSelectionActiveColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedNormalColor(
                        normalColor.x(), normalColor.y(), normalColor.z(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const int multiVertexEnd = std::clamp(
                        highlightLineMultiVertexCount_,
                        0,
                        highlightLineVertexCount_);
                    const bool occludedSecondaryEdgesValid =
                        secondaryEdgeHighlightVertexCount_ > 0
                        && secondaryEdgeHighlightFirstVertex_ >= 0
                        && secondaryEdgeHighlightFirstVertex_
                            + secondaryEdgeHighlightVertexCount_ <= multiVertexEnd;

                    highlightLineVao_->bind();
                    const std::size_t availableHighlightLineVertices = std::min(
                        static_cast<std::size_t>(std::max(0, highlightLineVertexCount_)),
                        highlightLineVertices_.size());
                    for (const render::DrawRange& range : geometrySelectionLineRanges_)
                    {
                        const render::SelectionWireRangeParts parts =
                            render::splitSelectionWireRange(range, multiVertexEnd);
                        if (render::isValidDrawRange(parts.multi, availableHighlightLineVertices))
                        {
                            thickLineProgram_->setUniformValue("u_color", occludedMultiColor);
                            thickLineProgram_->setUniformValue("u_line_width_px", 2.8f);
                            const int rangeStart = parts.multi.first;
                            const int rangeEnd = rangeStart + parts.multi.count;
                            const int selectedStart = secondaryEdgeHighlightFirstVertex_;
                            const int selectedEnd = selectedStart
                                + secondaryEdgeHighlightVertexCount_;
                            if (!occludedSecondaryEdgesValid
                                || selectedEnd <= rangeStart
                                || selectedStart >= rangeEnd)
                            {
                                f->glDrawArrays(GL_LINES, rangeStart, parts.multi.count);
                            }
                            else
                            {
                                if (selectedStart > rangeStart)
                                {
                                    f->glDrawArrays(
                                        GL_LINES, rangeStart,
                                        selectedStart - rangeStart);
                                }
                                if (selectedEnd < rangeEnd)
                                {
                                    f->glDrawArrays(
                                        GL_LINES, selectedEnd,
                                        rangeEnd - selectedEnd);
                                }
                            }
                        }
                        if (render::isValidDrawRange(parts.primary, availableHighlightLineVertices))
                        {
                            thickLineProgram_->setUniformValue("u_color", occludedPrimaryColor);
                            thickLineProgram_->setUniformValue("u_line_width_px", 3.2f);
                            const int rangeStart = parts.primary.first;
                            const int rangeEnd = rangeStart + parts.primary.count;
                            const int activeStart = activeEdgeHighlightFirstVertex_;
                            const int activeEnd = activeStart + activeEdgeHighlightVertexCount_;
                            if (activeEdgeHighlightVertexCount_ <= 0
                                || activeEnd <= rangeStart || activeStart >= rangeEnd)
                            {
                                f->glDrawArrays(GL_LINES, rangeStart, parts.primary.count);
                            }
                            else
                            {
                                if (activeStart > rangeStart)
                                {
                                    f->glDrawArrays(GL_LINES, rangeStart, activeStart - rangeStart);
                                }
                                if (activeEnd < rangeEnd)
                                {
                                    f->glDrawArrays(GL_LINES, activeEnd, rangeEnd - activeEnd);
                                }
                            }
                        }
                    }
                    if (occludedSecondaryEdgesValid)
                    {
                        thickLineProgram_->setUniformValue("u_color", occludedEdgeMultiColor);
                        thickLineProgram_->setUniformValue("u_line_width_px", 4.0f);
                        f->glDrawArrays(
                            GL_LINES,
                            secondaryEdgeHighlightFirstVertex_,
                            secondaryEdgeHighlightVertexCount_);
                    }
                    if (activeEdgeHighlightVertexCount_ > 0)
                    {
                        thickLineProgram_->setUniformValue("u_color", occludedActiveColor);
                        thickLineProgram_->setUniformValue("u_line_width_px", 4.4f);
                        f->glDrawArrays(
                            GL_LINES,
                            activeEdgeHighlightFirstVertex_,
                            activeEdgeHighlightVertexCount_);
                    }
                    if (normalsValid)
                    {
                        thickLineProgram_->setUniformValue("u_color", occludedNormalColor);
                        thickLineProgram_->setUniformValue("u_line_width_px", 3.0f);
                        f->glDrawArrays(
                            GL_LINES,
                            surfaceNormalHighlightFirstVertex_,
                            surfaceNormalHighlightVertexCount_);
                    }
                    highlightLineVao_->release();

                    if (geometrySelectionStencilActive)
                    {
                        f->glEnable(GL_STENCIL_TEST);
                        f->glStencilMask(0x00);
                        f->glStencilFunc(GL_ALWAYS, 0, geometryStencilBit);
                        f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    }
                    f->glDepthFunc(GL_LEQUAL);
                }
            }

            drawThingOverlayPasses(thickLineProgram_.get(), true);
            thickLineProgram_->setUniformValue("u_ndc_z_bias", 0.0f);
            thickLineProgram_->release();

            if (wireDepthPolicy.polygonOffset)
            {
                f->glDisable(GL_POLYGON_OFFSET_FILL);
            }
        }
        else
        {
            // Fallback: legacy GL lines.
            if (wireDepthPolicy.polygonOffset)
            {
                f->glEnable(GL_POLYGON_OFFSET_LINE);
                f->glPolygonOffset(
                    wireDepthPolicy.polygonOffsetFactor,
                    wireDepthPolicy.polygonOffsetUnits);
            }
            if (lineAa.legacyLineSmoothing)
            {
                f->glEnable(GL_LINE_SMOOTH);
                f->glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
            }
            else
            {
                f->glDisable(GL_LINE_SMOOTH);
            }
            f->glLineWidth(std::max(0.5f, wireframeLineWidthPx_));

            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue("u_geo_min", 0);
            program_->setUniformValue("u_color", lineColor);
            program_->setUniformValue("u_textured", 0);
            program_->setUniformValue("u_ndc_z_bias", wireDepthPolicy.ndcBias);

            if (canDrawLevelLines || canDrawThingLines)
            {
                if (geometrySelectionStencilActive)
                {
                    f->glDisable(GL_STENCIL_TEST);
                }
                lineVao_->bind();
                const bool useLayerColors = layerWireframeColoringEnabled_ && !baseLineLayerRanges_.empty();
                if (!useLayerColors)
                {
                    if (canDrawLevelLines)
                    {
                        f->glDrawArrays(GL_LINES, 0, wireLineVertexCount_);
                    }
                    if (canDrawThingLines)
                    {
                        f->glDrawArrays(GL_LINES, thingLineFirstVertex_, thingLineVertexCount_);
                    }
                }
                else
                {
                    const int drawFirst = canDrawLevelLines ? 0 : thingLineFirstVertex_;
                    const int drawEnd = canDrawThingLines
                        ? (thingLineFirstVertex_ + thingLineVertexCount_)
                        : wireLineVertexCount_;

                    for (const LayerLineRange& range : baseLineLayerRanges_)
                    {
                        const int start = std::max(drawFirst, range.first);
                        const int end = std::min(drawEnd, range.first + range.count);
                        const int count = end - start;
                        if (count <= 0)
                        {
                            continue;
                        }

                        program_->setUniformValue("u_color", layerColorVector(range.layer));
                        f->glDrawArrays(GL_LINES, start, count);
                    }
                }
                lineVao_->release();
                if (geometrySelectionStencilActive)
                {
                    f->glEnable(GL_STENCIL_TEST);
                    f->glStencilMask(0x00);
                    f->glStencilFunc(GL_NOTEQUAL, 1, geometryStencilBit);
                    f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                }
            }

            if (canDrawHighlightLines)
            {
                highlightLineVao_->bind();

                const int multiCount = std::clamp(
                    highlightLineMultiVertexCount_, 0, highlightLineVertexCount_);
                const int normalFirst = surfaceNormalHighlightFirstVertex_;
                const int normalEnd = normalFirst >= 0
                    && surfaceNormalHighlightVertexCount_
                        <= (std::numeric_limits<int>::max() - normalFirst)
                    ? normalFirst + surfaceNormalHighlightVertexCount_
                    : std::numeric_limits<int>::max();
                const bool normalsValid = render::isValidSelectionNormalRange(
                    render::DrawRange{
                        normalFirst, surfaceNormalHighlightVertexCount_ },
                    multiCount,
                    highlightLineVertexCount_);

                program_->setUniformValue("u_ndc_z_bias", selectionDepthPolicy.ndcBias);
                program_->setUniformValue("u_color", QVector4D(0.025f, 0.03f, 0.04f, 1.0f));
                f->glLineWidth(5.0f);
                // Keep the cyan normal marker out of the dark geometry halo;
                // draw it exactly once in the dedicated normal pass below.
                if (!normalsValid)
                {
                    f->glDrawArrays(GL_LINES, 0, highlightLineVertexCount_);
                }
                else
                {
                    if (normalFirst > 0)
                    {
                        f->glDrawArrays(GL_LINES, 0, normalFirst);
                    }
                    if (normalEnd < highlightLineVertexCount_)
                    {
                        f->glDrawArrays(
                            GL_LINES,
                            normalEnd,
                            highlightLineVertexCount_ - normalEnd);
                    }
                }
                if (geometrySelectionStencilActive)
                {
                    f->glStencilFunc(GL_ALWAYS, 0, geometryStencilBit);
                }

                const QVector4D multiColor(
                    effectiveSelectionMultiColor.redF(),
                    effectiveSelectionMultiColor.greenF(),
                    effectiveSelectionMultiColor.blueF(),
                    1.0f);
                const QVector4D edgeMultiColor(
                    effectiveSelectionEdgeMultiColor.redF(),
                    effectiveSelectionEdgeMultiColor.greenF(),
                    effectiveSelectionEdgeMultiColor.blueF(),
                    1.0f);
                const QVector4D primaryColor(
                    effectiveSelectionPrimaryColor.redF(),
                    effectiveSelectionPrimaryColor.greenF(),
                    effectiveSelectionPrimaryColor.blueF(),
                    1.0f);
                const QVector4D activeColor(
                    effectiveSelectionActiveColor.redF(),
                    effectiveSelectionActiveColor.greenF(),
                    effectiveSelectionActiveColor.blueF(),
                    1.0f);
                const QVector4D normalColor(0.12f, 0.92f, 1.0f, 1.0f);

                const bool secondaryEdgesValid =
                    secondaryEdgeHighlightVertexCount_ > 0
                    && secondaryEdgeHighlightFirstVertex_ >= 0
                    && secondaryEdgeHighlightFirstVertex_
                        + secondaryEdgeHighlightVertexCount_ == multiCount;
                const int multiBaseCount = secondaryEdgesValid
                    ? secondaryEdgeHighlightFirstVertex_ : multiCount;
                if (multiBaseCount > 0)
                {
                    program_->setUniformValue("u_color", multiColor);
                    f->glLineWidth(2.8f);
                    f->glDrawArrays(GL_LINES, 0, multiBaseCount);
                }
                if (secondaryEdgesValid)
                {
                    program_->setUniformValue("u_color", edgeMultiColor);
                    f->glLineWidth(4.0f);
                    f->glDrawArrays(
                        GL_LINES,
                        secondaryEdgeHighlightFirstVertex_,
                        secondaryEdgeHighlightVertexCount_);
                }

                const int primaryStart = multiCount;
                const int primaryEnd = normalsValid
                    ? normalFirst : highlightLineVertexCount_;

                const bool activeValid =
                    (activeEdgeHighlightVertexCount_ > 0)
                    && (activeEdgeHighlightFirstVertex_ >= 0)
                    && ((activeEdgeHighlightFirstVertex_ + activeEdgeHighlightVertexCount_) <= primaryEnd)
                    && (activeEdgeHighlightFirstVertex_ >= primaryStart);

                if (primaryEnd > primaryStart)
                {
                    if (!activeValid)
                    {
                        program_->setUniformValue("u_color", primaryColor);
                        f->glLineWidth(3.2f);
                        f->glDrawArrays(GL_LINES, primaryStart, primaryEnd - primaryStart);
                    }
                    else
                    {
                        const int baseCount = activeEdgeHighlightFirstVertex_ - primaryStart;
                        if (baseCount > 0)
                        {
                            program_->setUniformValue("u_color", primaryColor);
                            f->glLineWidth(3.2f);
                            f->glDrawArrays(GL_LINES, primaryStart, baseCount);
                        }

                        program_->setUniformValue("u_color", activeColor);
                        f->glLineWidth(4.4f);
                        f->glDrawArrays(GL_LINES, activeEdgeHighlightFirstVertex_, activeEdgeHighlightVertexCount_);

                        const int afterStart = activeEdgeHighlightFirstVertex_ + activeEdgeHighlightVertexCount_;
                        const int afterCount = primaryEnd - afterStart;
                        if (afterCount > 0)
                        {
                            program_->setUniformValue("u_color", primaryColor);
                            f->glLineWidth(3.2f);
                            f->glDrawArrays(GL_LINES, afterStart, afterCount);
                        }
                    }
                }

                if (normalsValid)
                {
                    program_->setUniformValue("u_color", normalColor);
                    f->glLineWidth(3.0f);
                    f->glDrawArrays(
                        GL_LINES,
                        surfaceNormalHighlightFirstVertex_,
                        surfaceNormalHighlightVertexCount_);
                }

                highlightLineVao_->release();

                // Legacy GL_LINES follows the same visible/occluded geometry
                // policy as the thick-line shader path. Restrict this pass to
                // selected Sector/Surface ranges so ordinary wireframe and
                // Thing highlights are not changed.
                const bool canDrawOccludedGeometryLines =
                    geometryStencilPolicy.drawOccludedWireframe
                    && !geometrySelectionLineRanges_.empty();
                if (canDrawOccludedGeometryLines)
                {
                    if (geometrySelectionStencilActive)
                    {
                        f->glDisable(GL_STENCIL_TEST);
                    }

                    f->glEnable(GL_BLEND);
                    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    f->glDepthMask(GL_FALSE);
                    f->glDepthFunc(GL_GREATER);
                    program_->setUniformValue("u_ndc_z_bias", 0.0f);
                    const QVector4D occludedMultiColor(
                        effectiveSelectionMultiColor.redF(),
                        effectiveSelectionMultiColor.greenF(),
                        effectiveSelectionMultiColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedEdgeMultiColor(
                        effectiveSelectionEdgeMultiColor.redF(),
                        effectiveSelectionEdgeMultiColor.greenF(),
                        effectiveSelectionEdgeMultiColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedPrimaryColor(
                        effectiveSelectionPrimaryColor.redF(),
                        effectiveSelectionPrimaryColor.greenF(),
                        effectiveSelectionPrimaryColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedActiveColor(
                        effectiveSelectionActiveColor.redF(),
                        effectiveSelectionActiveColor.greenF(),
                        effectiveSelectionActiveColor.blueF(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const QVector4D occludedNormalColor(
                        normalColor.x(), normalColor.y(), normalColor.z(),
                        geometryStencilPolicy.occludedWireframeAlpha);
                    const int multiVertexEnd = std::clamp(
                        highlightLineMultiVertexCount_,
                        0,
                        highlightLineVertexCount_);
                    const bool occludedSecondaryEdgesValid =
                        secondaryEdgeHighlightVertexCount_ > 0
                        && secondaryEdgeHighlightFirstVertex_ >= 0
                        && secondaryEdgeHighlightFirstVertex_
                            + secondaryEdgeHighlightVertexCount_ <= multiVertexEnd;

                    highlightLineVao_->bind();
                    const std::size_t availableHighlightLineVertices = std::min(
                        static_cast<std::size_t>(std::max(0, highlightLineVertexCount_)),
                        highlightLineVertices_.size());
                    for (const render::DrawRange& range : geometrySelectionLineRanges_)
                    {
                        const render::SelectionWireRangeParts parts =
                            render::splitSelectionWireRange(range, multiVertexEnd);
                        if (render::isValidDrawRange(parts.multi, availableHighlightLineVertices))
                        {
                            program_->setUniformValue("u_color", occludedMultiColor);
                            f->glLineWidth(2.8f);
                            const int rangeStart = parts.multi.first;
                            const int rangeEnd = rangeStart + parts.multi.count;
                            const int selectedStart = secondaryEdgeHighlightFirstVertex_;
                            const int selectedEnd = selectedStart
                                + secondaryEdgeHighlightVertexCount_;
                            if (!occludedSecondaryEdgesValid
                                || selectedEnd <= rangeStart
                                || selectedStart >= rangeEnd)
                            {
                                f->glDrawArrays(GL_LINES, rangeStart, parts.multi.count);
                            }
                            else
                            {
                                if (selectedStart > rangeStart)
                                {
                                    f->glDrawArrays(
                                        GL_LINES, rangeStart,
                                        selectedStart - rangeStart);
                                }
                                if (selectedEnd < rangeEnd)
                                {
                                    f->glDrawArrays(
                                        GL_LINES, selectedEnd,
                                        rangeEnd - selectedEnd);
                                }
                            }
                        }
                        if (render::isValidDrawRange(parts.primary, availableHighlightLineVertices))
                        {
                            program_->setUniformValue("u_color", occludedPrimaryColor);
                            f->glLineWidth(3.2f);
                            const int rangeStart = parts.primary.first;
                            const int rangeEnd = rangeStart + parts.primary.count;
                            const int activeStart = activeEdgeHighlightFirstVertex_;
                            const int activeEnd = activeStart + activeEdgeHighlightVertexCount_;
                            if (activeEdgeHighlightVertexCount_ <= 0
                                || activeEnd <= rangeStart || activeStart >= rangeEnd)
                            {
                                f->glDrawArrays(GL_LINES, rangeStart, parts.primary.count);
                            }
                            else
                            {
                                if (activeStart > rangeStart)
                                {
                                    f->glDrawArrays(GL_LINES, rangeStart, activeStart - rangeStart);
                                }
                                if (activeEnd < rangeEnd)
                                {
                                    f->glDrawArrays(GL_LINES, activeEnd, rangeEnd - activeEnd);
                                }
                            }
                        }
                    }
                    if (occludedSecondaryEdgesValid)
                    {
                        program_->setUniformValue("u_color", occludedEdgeMultiColor);
                        f->glLineWidth(4.0f);
                        f->glDrawArrays(
                            GL_LINES,
                            secondaryEdgeHighlightFirstVertex_,
                            secondaryEdgeHighlightVertexCount_);
                    }
                    if (activeEdgeHighlightVertexCount_ > 0)
                    {
                        program_->setUniformValue("u_color", occludedActiveColor);
                        f->glLineWidth(4.4f);
                        f->glDrawArrays(
                            GL_LINES,
                            activeEdgeHighlightFirstVertex_,
                            activeEdgeHighlightVertexCount_);
                    }
                    if (normalsValid)
                    {
                        program_->setUniformValue("u_color", occludedNormalColor);
                        f->glLineWidth(3.0f);
                        f->glDrawArrays(
                            GL_LINES,
                            surfaceNormalHighlightFirstVertex_,
                            surfaceNormalHighlightVertexCount_);
                    }
                    highlightLineVao_->release();

                    if (geometrySelectionStencilActive)
                    {
                        f->glEnable(GL_STENCIL_TEST);
                        f->glStencilMask(0x00);
                        f->glStencilFunc(GL_ALWAYS, 0, geometryStencilBit);
                        f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    }
                    f->glDepthFunc(GL_LEQUAL);
                }
            }

            drawThingOverlayPasses(program_.get(), false);

            if (wireDepthPolicy.polygonOffset)
            {
                f->glDisable(GL_POLYGON_OFFSET_LINE);
            }
        }

        program_->setUniformValue("u_ndc_z_bias", 0.0f);

        // Restore the exact entry state. In particular, do not infer blend or
        // multisample state from editor preferences: another overlay may have
        // intentionally installed a different state immediately before this
        // pass.
        const auto restoreCapability = [f](const GLenum capability, const bool enabled)
        {
            if (enabled)
            {
                f->glEnable(capability);
            }
            else
            {
                f->glDisable(capability);
            }
        };

        if (geometrySelectionStencilActive)
        {
            // Remove our mask before handing the stencil buffer back.  Only
            // bit zero was owned by this pass, and the normal frame path had
            // stencil disabled on entry.
            f->glStencilMask(geometryStencilBit);
            f->glClearStencil(0);
            f->glClear(GL_STENCIL_BUFFER_BIT);
        }
        f->glStencilMask(static_cast<GLuint>(linePreviousStencilWriteMask));
        f->glStencilFunc(
            static_cast<GLenum>(linePreviousStencilFunc),
            linePreviousStencilRef,
            static_cast<GLuint>(linePreviousStencilValueMask));
        f->glStencilOp(
            static_cast<GLenum>(linePreviousStencilFail),
            static_cast<GLenum>(linePreviousStencilDepthFail),
            static_cast<GLenum>(linePreviousStencilDepthPass));
        f->glClearStencil(linePreviousStencilClearValue);
        restoreCapability(GL_STENCIL_TEST, lineStencilWasEnabled);
        f->glColorMask(
            linePreviousColorMask[0],
            linePreviousColorMask[1],
            linePreviousColorMask[2],
            linePreviousColorMask[3]);

        f->glBlendFuncSeparate(
            static_cast<GLenum>(linePreviousBlendSrcRgb),
            static_cast<GLenum>(linePreviousBlendDstRgb),
            static_cast<GLenum>(linePreviousBlendSrcAlpha),
            static_cast<GLenum>(linePreviousBlendDstAlpha));
        restoreCapability(GL_BLEND, lineBlendWasEnabled);
        restoreCapability(GL_MULTISAMPLE, lineMultisampleWasEnabled);
        f->glHint(GL_LINE_SMOOTH_HINT, static_cast<GLenum>(linePreviousLineSmoothHint));
        restoreCapability(GL_LINE_SMOOTH, lineSmoothWasEnabled);
        f->glLineWidth(linePreviousWidth);

        f->glCullFace(static_cast<GLenum>(linePreviousCullMode));
        f->glFrontFace(static_cast<GLenum>(linePreviousFrontFace));
        restoreCapability(GL_CULL_FACE, lineCullWasEnabled);

        f->glPolygonOffset(
            linePreviousPolygonOffsetFactor,
            linePreviousPolygonOffsetUnits);
        restoreCapability(GL_POLYGON_OFFSET_POINT, linePolygonPointWasEnabled);
        restoreCapability(GL_POLYGON_OFFSET_FILL, linePolygonFillWasEnabled);
        restoreCapability(GL_POLYGON_OFFSET_LINE, linePolygonLineWasEnabled);

        f->glDepthMask(linePreviousDepthMask);
        f->glDepthFunc(static_cast<GLenum>(linePreviousDepthFunc));
        restoreCapability(GL_DEPTH_TEST, lineDepthTestWasEnabled);
    }

    if (hasOverlayPrimitives)
    {
        // The point/line overlay temporarily installs a read-only LEQUAL
        // depth state. Restore the state from before the whole overlay block,
        // even when there are no later gizmos to establish frame defaults.
        f->glDepthMask(overlayPreviousDepthMask);
        f->glDepthFunc(static_cast<GLenum>(overlayPreviousDepthFunc));
        if (overlayDepthTestWasEnabled)
        {
            f->glEnable(GL_DEPTH_TEST);
        }
        else
        {
            f->glDisable(GL_DEPTH_TEST);
        }
    }

    program_->release();
    drawLightGizmos(mvp);
    drawThingGizmos(mvp);
    drawArchitecturalLightingOwnerGizmos(mvp);
    drawSelectedLightRange(mvp);
    drawSectorThrustGizmos(mvp, eye);
    drawTransformGizmo(proj, view, eye, target);

    // Thing-frame/path edit overlay: draw frame points/segments for the active thing.
    if (frameEditEnabled_ && level_)
    {
        auto activeFrameThingIndex = [&]() -> std::int32_t
        {
            if (selectedThing_ >= 0 && static_cast<std::size_t>(selectedThing_) < level_->things.size())
            {
                const Thing& selected = level_->things[static_cast<std::size_t>(selectedThing_)];
                if (isLayerVisible(selected.layer) && shouldShowThing(selected))
                {
                    return selectedThing_;
                }
            }

            std::int32_t best = -1;
            for (const sed::level::FrameRef& ref : highlightedSelection_.frames)
            {
                if (ref.thing < 0 || static_cast<std::size_t>(ref.thing) >= level_->things.size())
                {
                    continue;
                }
                const Thing& thing = level_->things[static_cast<std::size_t>(ref.thing)];
                if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
                {
                    continue;
                }
                if (best < 0 || ref.thing < best)
                {
                    best = ref.thing;
                }
            }
            return best;
        }();

        if (activeFrameThingIndex >= 0 && static_cast<std::size_t>(activeFrameThingIndex) < level_->things.size())
        {
            const Thing& thing = level_->things[static_cast<std::size_t>(activeFrameThingIndex)];

            auto activeFrameValueIndex = [&]() -> std::int32_t
            {
                if (selectedFrameValueIndex_ >= 0
                    && static_cast<std::size_t>(selectedFrameValueIndex_) < thing.values.size())
                {
                    const TplValue& selectedValue = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
                    if (selectedValue.type == ValueType::frame && sed::text::iequalsAscii(selectedValue.name, "frame"))
                    {
                        return selectedFrameValueIndex_;
                    }
                }

                std::int32_t best = -1;
                for (const sed::level::FrameRef& ref : highlightedSelection_.frames)
                {
                    if (ref.thing != activeFrameThingIndex || ref.value < 0)
                    {
                        continue;
                    }
                    if (static_cast<std::size_t>(ref.value) >= thing.values.size())
                    {
                        continue;
                    }

                    const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
                    if (value.type != ValueType::frame || !sed::text::iequalsAscii(value.name, "frame"))
                    {
                        continue;
                    }

                    if (best < 0 || ref.value < best)
                    {
                        best = ref.value;
                    }
                }
                return best;
            }();

            FrameOwnerPoseCache* const ownerCache =
                ensureFrameOwnerPoseCache(activeFrameThingIndex);
            const std::vector<render::frame_ghost::FramePose>* cachedPoses =
                ownerCache
                    ? &ownerCache->poses
                    : nullptr;
            const auto framePoseAt = [cachedPoses](const std::size_t index)
                -> const render::frame_ghost::FramePose&
            {
                return (*cachedPoses)[index];
            };
            const std::size_t pathPoseCount = cachedPoses
                ? cachedPoses->size()
                : 0u;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            frameOverlayWorkForTest_.lastFramePathPoseCount = pathPoseCount;
#endif

            const auto frameWorldPosition = [this, activeFrameThingIndex](
                                                const render::frame_ghost::FramePose& pose)
            {
                QVector3D world(
                    static_cast<float>(pose.x),
                    static_cast<float>(pose.y),
                    static_cast<float>(pose.z));
                if ( transformDrag_.active
                    && transformDrag_.target == TransformTargetKind::Frame
                    && transformDrag_.frameThing == activeFrameThingIndex
                    && transformDrag_.frameValue == pose.valueIndex
                    && (transformDrag_.handle == TransformHandle::MoveX
                        || transformDrag_.handle == TransformHandle::MoveY
                        || transformDrag_.handle == TransformHandle::MoveZ
                        || transformDrag_.handle == TransformHandle::MoveFree) )
                {
                    world += transformDrag_.translation;
                }
                return world;
            };

            if ( pathPoseCount > 0u )
            {
                const std::shared_ptr<const assets::Model3do> frameModel = ownerCache
                    ? ensureFrameOwnerModel(*ownerCache, thing)
                    : std::shared_ptr<const assets::Model3do>{};
                FrameModelTopologyCache* topologyCache = frameModel
                    ? ensureFrameModelTopologyCache(frameModel)
                    : nullptr;

                // QPainter overlays need a clean baseline after the GL gizmo/shader passes.
                f->glDisable(GL_DEPTH_TEST);
                f->glDisable(GL_STENCIL_TEST);
                f->glDisable(GL_SCISSOR_TEST);
                f->glDisable(GL_BLEND);
                f->glDisable(GL_CULL_FACE);
                f->glDepthMask(GL_TRUE);
                f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                f->glStencilMask(0xFF);

                const bool gpuGhostReady = frameGhostProgram_
                    && frameGhostLineVao_
                    && frameGhostLineVbo_;
                const bool gpuGhostUsable = gpuGhostReady
                    && cachedPoses
                    && ownerCache
                    && frameModel
                    && topologyCache
                    && topologyCache->topology.withinBudget
                    && topologyCache->haveFiniteBounds
                    && !topologyCache->lineVertices.empty();
                if ( gpuGhostUsable )
                {
                    drawFrameGhostBatch(*ownerCache, activeFrameValueIndex, mvp);
                }

                bool invMvpOk = false;
                const QMatrix4x4 invMvp = mvp.inverted(&invMvpOk);

                auto projectedFramePoint = [&](const QVector3D& world) -> std::optional<QPointF>
                {
                    return render::frame_path::projectWorldPoint(
                        mvp,
                        world,
                        width(),
                        height());
                };

                struct ClippedFrameSegment
                {
                    QPointF a{};
                    QPointF b{};
                    QVector3D midWorld{};
                };

                auto clippedFrameSegment = [&](const QVector3D& worldA, const QVector3D& worldB) -> std::optional<ClippedFrameSegment>
                {
                    const std::optional<render::frame_path::ProjectedSegment> projected =
                        render::frame_path::projectWorldSegment(
                            mvp,
                            worldA,
                            worldB,
                            width(),
                            height());
                    if (!projected)
                    {
                        return std::nullopt;
                    }

                    const float midT = (projected->firstT + projected->secondT) * 0.5f;
                    const QVector3D midWorld = worldA + (worldB - worldA) * midT;

                    return ClippedFrameSegment{
                        projected->first,
                        projected->second,
                        midWorld
                    };
                };

                auto worldSizeToPixels = [&](const QVector3D& world, const float worldSize, const float minPx, const float maxPx) -> float
                {
                    if (!invMvpOk)
                    {
                        return std::clamp(worldSize * 120.0f, minPx, maxPx);
                    }

                    const float unitsPerPx = worldUnitsPerPixelAt(world, mvp, invMvp, width(), height());
                    if (!std::isfinite(unitsPerPx) || unitsPerPx <= 1.0e-6f)
                    {
                        return std::clamp(worldSize * 120.0f, minPx, maxPx);
                    }

                    return std::clamp(worldSize / unitsPerPx, minPx, maxPx);
                };

                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);

                if ( frameModel && !gpuGhostUsable
                    && topologyCache && topologyCache->topology.withinBudget )
                {
                    QPen wirePen(QColor(200, 210, 220, 80), 1.0);
                    wirePen.setCosmetic(true);
                    painter.setPen(wirePen);
                    painter.setBrush(Qt::NoBrush);

                    for ( std::size_t poseIndex = 0u;
                        poseIndex < pathPoseCount;
                        ++poseIndex )
                    {
                        const render::frame_ghost::FramePose& pose =
                            framePoseAt(poseIndex);
                        if ( pose.valueIndex == activeFrameValueIndex
                            || !pose.finiteOrientation )
                        {
                            continue;
                        }
                        TVector pyr{};
                        pyr.pitch = pose.pitch;
                        pyr.yaw = pose.yaw;
                        pyr.roll = pose.roll;
                        const QVector3D world = frameWorldPosition(pose);
                        const TMatrix43 transform = TMatrix43::Create(
                            TVector{ world.x(), world.y(), world.z() },
                            pyr);
                        for ( std::size_t edgeIndex = 0u;
                            edgeIndex + 1u < topologyCache->lineVertices.size();
                            edgeIndex += 2u )
                        {
                            const render::frame_ghost::LocalVertex& first =
                                topologyCache->lineVertices[edgeIndex];
                            const render::frame_ghost::LocalVertex& second =
                                topologyCache->lineVertices[edgeIndex + 1u];
                            const TVector worldFirst = transform.TransformPoint(
                                TVector{ first[0], first[1], first[2] });
                            const TVector worldSecond = transform.TransformPoint(
                                TVector{ second[0], second[1], second[2] });
                            const auto edge = render::frame_path::projectWorldSegment(
                                mvp,
                                QVector3D(worldFirst.x, worldFirst.y, worldFirst.z),
                                QVector3D(worldSecond.x, worldSecond.y, worldSecond.z),
                                width(),
                                height());
                            if ( edge )
                            {
                                painter.drawLine(edge->first, edge->second);
                            }
                        }
                    }
                }
                else if ( frameModel && !gpuGhostUsable
                    && (!topologyCache || !topologyCache->topology.withinBudget) )
                {
                    // A cached negative topology is not retried or walked on
                    // the paint path. Frame points remain available below.
                }

                painter.setBrush(Qt::NoBrush);

                for ( std::size_t i = 1u; i < pathPoseCount; ++i )
                {
                    const QVector3D previousWorld = frameWorldPosition(
                        framePoseAt(i - 1u));
                    const QVector3D currentWorld = frameWorldPosition(
                        framePoseAt(i));
                    const std::optional<ClippedFrameSegment> seg =
                        clippedFrameSegment(previousWorld, currentWorld);
                    if (!seg)
                    {
                        continue;
                    }

                    QPen linePen(QColor(242, 244, 248, 210), worldSizeToPixels(seg->midWorld, framePathLineWorldSize_, 1.0f, 18.0f));
                    linePen.setStyle(Qt::SolidLine);
                    linePen.setCapStyle(Qt::RoundCap);
                    linePen.setJoinStyle(Qt::RoundJoin);
                    painter.setPen(linePen);
                    painter.drawLine(seg->a, seg->b);
                }

                const QColor normalFill(245, 246, 250, 235);
                QColor activeFill(230, 54, 54, 245);
                const QColor outline(0, 0, 0, 200);

                for ( std::size_t poseIndex = 0u;
                    poseIndex < pathPoseCount;
                    ++poseIndex )
                {
                    const render::frame_ghost::FramePose& pose =
                        framePoseAt(poseIndex);
                    const QVector3D world = frameWorldPosition(pose);
                    const std::optional<QPointF> screen = projectedFramePoint(world);
                    if (!screen)
                    {
                        continue;
                    }

                    const bool active = pose.valueIndex == activeFrameValueIndex;
                    const float baseR = worldSizeToPixels(
                        world,
                        framePathPointWorldSize_,
                        2.0f,
                        40.0f);
                    const float r = active ? (baseR * 1.35f) : baseR;

                    painter.setPen(QPen(outline, 1.2));
                    painter.setBrush(active ? activeFill : normalFill);
                    painter.drawEllipse(*screen, r, r);
                }
            }
        }
    }

    if (sectorThrustToolEnabled_ && !sectorThrustLabels_.empty())
    {
        // Labels are admitted only after their world anchor passes the same
        // solid-surface visibility test as the GL arrow. Keep them as a 2D
        // overlay for crisp, screen-constant magnitude text.
        f->glDisable(GL_DEPTH_TEST);
        f->glDisable(GL_STENCIL_TEST);
        f->glDisable(GL_SCISSOR_TEST);
        f->glDisable(GL_BLEND);
        f->glDisable(GL_CULL_FACE);
        f->glDepthMask(GL_TRUE);
        f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font = painter.font();
        font.setPointSizeF(std::max(8.0, font.pointSizeF() - 0.5));
        font.setBold(true);
        painter.setFont(font);

        for (const SectorThrustLabel& label : sectorThrustLabels_)
        {
            const QRectF textBounds = painter.boundingRect(QRectF(), Qt::AlignLeft, label.text);
            const QRectF pill(
                label.position.x() - 5.0,
                label.position.y() - 3.0,
                textBounds.width() + 10.0,
                textBounds.height() + 6.0);
            painter.setPen(QPen(label.selected ? QColor(255, 112, 96, 220) : QColor(226, 54, 48, 155), 1.0));
            painter.setBrush(QColor(20, 10, 11, label.selected ? 220 : 185));
            painter.drawRoundedRect(pill, 5.0, 5.0);
            painter.setPen(label.selected ? QColor(255, 185, 176) : QColor(245, 125, 112));
            painter.drawText(pill, Qt::AlignCenter, label.text);
        }
    }

    if (showAxisWidget_)
    {
        const QRect axisRect = axisWidgetRectForViewport(this, width(), height());
        const AxisWidgetLayout axis = computeAxisWidgetLayout(camera_.viewMatrix(), axisRect);
        if (axis.rect.isValid())
        {
            auto axisColor = [](const int a) -> QColor
            {
                switch (a)
                {
                    case 0: return QColor(210, 75, 70);
                    case 1: return QColor(95, 190, 95);
                    default: return QColor(90, 140, 215);
                }
            };

            // Reset GL state so QPainter's internal paint engine starts cleanly.
            // Without this, leftover state from prior shader passes (depth/stencil/blend/scissor)
            // can prevent QPainter from rendering filled shapes on the first frame or after
            // loading a new project.
            f->glDisable(GL_DEPTH_TEST);
            f->glDisable(GL_STENCIL_TEST);
            f->glDisable(GL_SCISSOR_TEST);
            f->glDisable(GL_BLEND);
            f->glDisable(GL_CULL_FACE);
            f->glDepthMask(GL_TRUE);
            f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            f->glStencilMask(0xFF);

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);

            // Background circle.
            painter.setPen(QPen(QColor(255, 255, 255, 40), 1.0));
            painter.setBrush(QColor(28, 28, 32, 180));
            painter.drawEllipse(axis.center, axis.outerRadius, axis.outerRadius);

            // Axis lines.
            for (int a = 0; a < 3; ++a)
            {
                const AxisWidgetEnd& e0 = axis.ends[static_cast<std::size_t>(a) * 2 + 0];
                const AxisWidgetEnd& e1 = axis.ends[static_cast<std::size_t>(a) * 2 + 1];

                QColor c = axisColor(a);
                c.setAlpha(140);
                QPen pen(c, 2.0);
                pen.setCapStyle(Qt::RoundCap);
                painter.setPen(pen);
                painter.drawLine(e0.pos, e1.pos);
            }

            // Handles: draw back-to-front by depth so front caps are on top.
            std::vector<AxisWidgetEnd> ends{};
            ends.reserve(6);
            for (int a = 0; a < 3; ++a)
            {
                ends.push_back(axis.ends[static_cast<std::size_t>(a) * 2 + 0]);
                ends.push_back(axis.ends[static_cast<std::size_t>(a) * 2 + 1]);
            }

            std::sort(ends.begin(), ends.end(), [](const AxisWidgetEnd& a, const AxisWidgetEnd& b)
                {
                    return a.depth < b.depth;
                });

            QFont font = painter.font();
            font.setPointSizeF(std::max(7.0, font.pointSizeF() - 1.0));
            font.setBold(true);
            painter.setFont(font);

            for (const AxisWidgetEnd& e : ends)
            {
                const float r = e.front ? axis.frontRadius : axis.backRadius;
                QColor c = axisColor(e.axis);
                if (!e.front)
                {
                    c = c.darker(150);
                    c.setAlpha(140);
                }

                painter.setPen(Qt::NoPen);
                painter.setBrush(c);
                painter.drawEllipse(e.pos, r, r);

                if (e.front)
                {
                    painter.setPen(QColor(245, 245, 245, 230));
                    const QString label = (e.sign < 0 ? "-" : "") + QString::fromLatin1(e.axis == 0 ? "X" : (e.axis == 1 ? "Y" : "Z"));
                    const QRectF tr(e.pos.x() - r, e.pos.y() - r, r * 2.0f, r * 2.0f);
                    painter.drawText(tr, Qt::AlignCenter, label);
                }
            }
        }
    }

    if (boxSelectVisible_)
    {
        const QRect r = QRect(boxSelectStart_, boxSelectEnd_).normalized();
        if (r.isValid())
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);

            QPen pen(QColor(170, 200, 255, 220));
            pen.setWidthF(1.5);
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(QColor(90, 140, 255, 35));
            painter.drawRoundedRect(r, 4.0, 4.0);
        }
    }

    if ( stagedCleaveMarqueeSelection_.active
        && render::cleavePointerMotionIsDrag(
            stagedCleaveMarqueeSelection_.currentPosition
                - stagedCleaveMarqueeSelection_.startPosition) )
    {
        const QRect area(
            stagedCleaveMarqueeSelection_.startPosition,
            stagedCleaveMarqueeSelection_.currentPosition);
        if ( area.isValid() )
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            const bool removing = stagedCleaveMarqueeSelection_.mode
                == SelectionBoxMode::Remove;
            const QColor accent = removing
                ? QColor(244, 132, 112, 225)
                : QColor(170, 200, 255, 225);
            QPen pen(accent);
            pen.setWidthF(1.5);
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(removing
                ? QColor(244, 112, 90, 38)
                : QColor(90, 140, 255, 38));
            painter.drawRect(area.normalized());
        }
    }

    const bool cleaveOwnerVisible = renderPipeline_ == RenderPipeline::Editor
        && (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_);
    if (cleaveOwnerVisible
        && ((sectorCleaveToolEnabled_ && sectorCleaveHaveFirstPoint_)
        || (surfaceCleaveToolEnabled_ && surfaceCleaveHaveFirstPoint_)
        || (isStagedCleaveInteractionActive() && !stagedCleaveLines_.empty())
        || (isStagedCleaveInteractionActive() && cleaveShapeOptions_.placementArmed
            && stagedCleaveShapePlacementPlan_
            && stagedCleaveShapePreviewFrame_)
        || cleaveSnapMarker_
        || cleaveSnapGuidesVisible_))
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        // Most paints use the live lines directly. Only an endpoint drag needs
        // a render-only reshape, since its authored endpoints commit on release.
        std::span<const StagedCleaveLine> displayedCleaveLines =
            isStagedCleaveInteractionActive()
            ? std::span<const StagedCleaveLine>(stagedCleaveLines_)
            : std::span<const StagedCleaveLine>{};
        std::vector<StagedCleaveLine> endpointPreviewLines{};
        if (editingStagedCleaveLine_ >= 0
            && editingStagedCleaveEndpoint_ >= 0
            && static_cast<std::size_t>(editingStagedCleaveLine_) < displayedCleaveLines.size())
        {
            endpointPreviewLines.assign(displayedCleaveLines.begin(), displayedCleaveLines.end());
            const std::size_t index = static_cast<std::size_t>(editingStagedCleaveLine_);
            const TVector position = sectorCleaveToolEnabled_
                ? sectorCleaveCurrentPoint_
                : surfaceCleaveCurrentPoint_;
            (void)reshapeStagedCleaveEndpointWithOptionalDisjoint(
                endpointPreviewLines,
                index,
                editingStagedCleaveEndpoint_,
                position,
                editingDisjointStagedCleaveEndpoint_);
            displayedCleaveLines = endpointPreviewLines;
        }

        // Joint measurements are world geometry. Camera movement only projects
        // these guides; authoring and actual drag-preview changes rebuild them.
        auto& jointGuides = stagedCleaveJointGuides_;
        if (cleaveAnglesVisible_
            && (jointGuides.authoredRevision != stagedCleaveAuthoredRevision_
                || jointGuides.previewRevision != stagedCleaveJointGuidePreviewRevision_))
        {
            constexpr std::size_t kJointPresentationBudget = 16u * 1024u * 1024u;
            const std::size_t retained = jointGuides.joints.capacity() * sizeof(render::CleaveJointAngle)
                + jointGuides.linesWithJoint.capacity() * sizeof(std::uint8_t);
            jointGuides.joints.clear();
            jointGuides.linesWithJoint.clear();
            if (render::cleaveJointGuideRebuildFitsBudget(
                    displayedCleaveLines.size(), retained, kJointPresentationBudget))
            {
                try
                {
                    auto joints = render::makeCleaveJointAngleMeasurements(displayedCleaveLines);
                    std::vector<std::uint8_t> masks(displayedCleaveLines.size(), 0u);
                    for (const auto& joint : joints)
                    {
                        for (const std::size_t line : {joint.firstLine, joint.secondLine})
                        {
                            masks[line] |= render::cleaveSameHandle(
                                displayedCleaveLines[line].first, joint.joint) ? 1u : 2u;
                        }
                    }
                    jointGuides.joints = std::move(joints);
                    jointGuides.linesWithJoint = std::move(masks);
                }
                catch (const std::bad_alloc&)
                {
                    // Guides are presentation-only. Keep the existing bearing
                    // fallback without changing draft/history admission.
                }
            }
            // Budget failure is stable until geometry changes, avoiding an
            // allocation retry on every repaint or camera sample.
            jointGuides.authoredRevision = stagedCleaveAuthoredRevision_;
            jointGuides.previewRevision = stagedCleaveJointGuidePreviewRevision_;
        }

        auto stagedLineColor = [&](const std::size_t index) -> QColor
        {
            static const std::array<QColor, 6> kSectorColors{
                QColor(112, 190, 255, 230),
                QColor(88, 224, 194, 230),
                QColor(190, 144, 255, 230),
                QColor(255, 146, 112, 230),
                QColor(128, 216, 112, 230),
                QColor(238, 128, 196, 230),
            };
            static const std::array<QColor, 6> kSurfaceColors{
                QColor(255, 150, 202, 230),
                QColor(180, 148, 255, 230),
                QColor(255, 174, 112, 230),
                QColor(112, 204, 255, 230),
                QColor(242, 128, 142, 230),
                QColor(132, 220, 178, 230),
            };
            const auto& colors = sectorCleaveToolEnabled_ ? kSectorColors : kSurfaceColors;
            return colors[index % colors.size()];
        };

        auto drawLabel = [&](const QPointF& center, const QString& text, const QColor& accent)
        {
            const QFontMetrics metrics(painter.font());
            QRectF rect(metrics.boundingRect(text));
            rect.adjust(-5.0, -2.0, 5.0, 2.0);
            rect.moveCenter(center);
            painter.setPen(QPen(accent, 1.0));
            painter.setBrush(QColor(18, 19, 23, 225));
            painter.drawRoundedRect(rect, 4.0, 4.0);
            painter.setPen(QColor(245, 247, 250, 240));
            painter.drawText(rect, Qt::AlignCenter, text);
        };

        auto drawCleaveLine = [&](const TVector& a3,
                                  const TVector& b3,
                                  const QColor& baseColor,
                                  const bool selected,
                                  const std::optional<std::size_t> stagedIndex,
                                  const StagedCleaveLine* previewFrame)
        {
            // QPainter will clip its final pixels to the widget, but it cannot
            // repair a perspective divide which happened on the wrong side of
            // the camera plane.  Cleave endpoints are frequently authored on
            // sector edges, so a line can have one endpoint outside any of
            // the six frustum planes (or behind the camera) while the other
            // remains visible.  Projecting both raw endpoints in that case
            // can flip one point to a huge screen coordinate and produces the
            // characteristic flashing diagonal across the viewport.  Reuse
            // the homogeneous/frustum clip used by frame/path overlays and
            // draw only the finite, visible portion of the segment.
            const std::optional<render::frame_path::ProjectedSegment> projected =
                render::frame_path::projectWorldSegment(
                    mvp,
                    a3,
                    b3,
                    width(),
                    height());
            if (!projected)
            {
                return;
            }

            const QPointF a = projected->first;
            const QPointF b = projected->second;

            const QColor c = selected ? QColor(255, 210, 72, 245) : baseColor;
            QPen pen(c);
            pen.setWidthF(selected ? 3.0 : 2.0);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(a, b);

            painter.setPen(Qt::NoPen);
            painter.setBrush(c);
            painter.drawEllipse(a, selected ? 5.0 : 3.5, selected ? 5.0 : 3.5);
            painter.drawEllipse(b, selected ? 5.0 : 3.5, selected ? 5.0 : 3.5);

            const QPointF midpoint = (a + b) * 0.5;
            QPointF currentDirection = b - a;
            const double currentLength = std::hypot(
                currentDirection.x(), currentDirection.y());
            QPointF lineNormal{};
            if (currentLength > 1.0e-6)
            {
                currentDirection /= currentLength;
                lineNormal = QPointF(-currentDirection.y(), currentDirection.x());
            }

            const bool showLength = cleaveLengthsVisible_;
            if (showLength)
            {
                drawLabel(
                    midpoint,
                    render::cleaveLengthText(
                        static_cast<double>((b3 - a3).length()),
                        cleaveMeasureUnit_),
                    c);
            }
            if (cleaveLineNumbersVisible_ && cleaveStagedMode_)
            {
                if ( stagedIndex && *stagedIndex < stagedCleaveLines_.size() )
                {
                    auto* const mutableThis = const_cast<GlView*>(this);
                    if ( !mutableThis->stagedCleaveRankCacheValid_ )
                    {
                        mutableThis->rebuildStagedCleaveRankCache();
                    }
                    const std::int32_t rank = *stagedIndex
                            < stagedCleaveRankByLine_.size()
                        ? stagedCleaveRankByLine_[*stagedIndex]
                        : -1;
                    const QRect labelRect = stagedCleaveOrderLabelRectForSegment(
                        a3,
                        b3,
                        rank,
                        mvp);
                    if ( !labelRect.isEmpty() )
                    {
                        painter.setPen(QPen(c, 1.0));
                        painter.setBrush(QColor(18, 19, 23, 225));
                        painter.drawRoundedRect(
                            QRectF(labelRect),
                            4.0,
                            4.0);
                        painter.setPen(QColor(245, 247, 250, 240));
                        painter.drawText(
                            QRectF(labelRect),
                            Qt::AlignCenter,
                            render::cleaveLineNumberText(
                                static_cast<std::size_t>(rank)));
                    }
                }
            }

            if (!cleaveAnglesVisible_
                || (stagedIndex && *stagedIndex < jointGuides.linesWithJoint.size()
                    && (jointGuides.linesWithJoint[*stagedIndex] & 1u) != 0u))
            {
                return;
            }

            double angle = render::cleaveScreenAngleDegrees(a, b);
            QPointF referenceDirection(1.0, 0.0);
            bool haveReference = false;
            const StagedCleaveLine* angleFrame = previewFrame;
            if (!angleFrame && stagedIndex && *stagedIndex < displayedCleaveLines.size())
            {
                angleFrame = &displayedCleaveLines[*stagedIndex];
            }
            if (angleFrame)
            {
                if (angleFrame->linkedToPrevious && !displayedCleaveLines.empty())
                {
                    const std::size_t previousIndex = stagedIndex
                        ? (*stagedIndex == 0 ? displayedCleaveLines.size() - 1 : *stagedIndex - 1)
                        : displayedCleaveLines.size() - 1;
                    const StagedCleaveLine& previous = displayedCleaveLines[previousIndex];
                    angle = render::cleaveAngleBetweenDegrees(
                        previous.first - a3,
                        b3 - a3);
                    const std::optional<QPointF> referencePoint = render::frame_path::projectWorldPoint(
                        mvp,
                        previous.first,
                        width(),
                        height());
                    if (referencePoint)
                    {
                        referenceDirection = *referencePoint - a;
                        haveReference = true;
                    }
                }
                else
                {
                    angle = render::cleaveStableAngleDegrees(
                        b3 - a3,
                        angleFrame->angleReferenceAxis,
                        angleFrame->constructionPlaneNormal);
                    const double guideLength = std::max(0.01, (b3 - a3).length() * 0.35);
                    const std::optional<QPointF> referencePoint = render::frame_path::projectWorldPoint(
                        mvp,
                        a3 + angleFrame->angleReferenceAxis * guideLength,
                        width(),
                        height());
                    if (referencePoint)
                    {
                        referenceDirection = *referencePoint - a;
                        haveReference = true;
                    }
                }
            }

            const double referenceLength = std::hypot(referenceDirection.x(), referenceDirection.y());
            if (currentLength > 1.0e-6
                && (!angleFrame || (haveReference && referenceLength > 1.0e-6)))
            {
                referenceDirection /= std::max(1.0e-6, referenceLength);
                const double startAngle = std::atan2(referenceDirection.y(), referenceDirection.x());
                const double endAngle = std::atan2(currentDirection.y(), currentDirection.x());
                double sweep = endAngle - startAngle;
                constexpr double kPi = 3.14159265358979323846;
                while (sweep > kPi)
                {
                    sweep -= 2.0 * kPi;
                }
                while (sweep < -kPi)
                {
                    sweep += 2.0 * kPi;
                }

                if (angleFrame)
                {
                    constexpr double kArcRadius = 18.0;
                    constexpr int kArcSteps = 18;
                    QPainterPath arc{};
                    for (int step = 0; step <= kArcSteps; ++step)
                    {
                        const double t = static_cast<double>(step) / static_cast<double>(kArcSteps);
                        const double arcAngle = startAngle + sweep * t;
                        const QPointF point = a + QPointF(
                            std::cos(arcAngle) * kArcRadius,
                            std::sin(arcAngle) * kArcRadius);
                        if (step == 0)
                        {
                            arc.moveTo(point);
                        }
                        else
                        {
                            arc.lineTo(point);
                        }
                    }
                    QPen arcPen(c);
                    arcPen.setWidthF(1.25);
                    painter.setPen(arcPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPath(arc);
                    const double middleAngle = startAngle + sweep * 0.5;
                    drawLabel(
                        a + QPointF(
                            std::cos(middleAngle) * 34.0,
                            std::sin(middleAngle) * 34.0),
                        render::compactCleaveMeasurementNumber(angle, 1) + QChar(0x00b0),
                        c);
                }
                else
                {
                    const QPointF normal(-currentDirection.y(), currentDirection.x());
                    drawLabel(
                        a + currentDirection * 22.0 + normal * 14.0,
                        render::compactCleaveMeasurementNumber(angle, 1) + QChar(0x00b0),
                        c);
                }
            }
        };

        auto drawSectorCleavePlane = [&](const StagedCleaveLine& line,
                                         const QColor& baseColor,
                                         const bool selected)
        {
            QColor planeColor = baseColor;
            planeColor.setAlpha(selected ? 52 : 30);
            QColor planeEdge = selected
                ? QColor(255, 210, 72, 210)
                : baseColor;
            planeEdge.setAlpha(selected ? 210 : 140);
            QPen planePen(planeEdge);
            planePen.setWidthF(selected ? 1.7 : 1.15);
            planePen.setStyle(Qt::DashLine);

            const std::optional<render::frame_path::ProjectedSegment> projectedLine =
                render::frame_path::projectWorldSegment(
                    mvp,
                    line.first,
                    line.second,
                    width(),
                    height());
            const bool haveLineMidpoint = projectedLine.has_value();
            const QPointF lineMidpoint = haveLineMidpoint
                ? (projectedLine->first + projectedLine->second) * 0.5
                : QPointF{};

            for (const std::vector<TVector>& polygon3d
                 : stagedCleavePlanePolygons(line))
            {
                QPolygonF polygon2d{};
                bool validPolygon = true;
                for (const TVector& point : polygon3d)
                {
                    const std::optional<QPointF> projected = render::frame_path::projectWorldPoint(
                        mvp,
                        point,
                        width(),
                        height());
                    if (!projected)
                    {
                        validPolygon = false;
                        break;
                    }
                    polygon2d.push_back(*projected);
                }
                if (!validPolygon || polygon2d.size() < 3)
                {
                    // A plane polygon may straddle the camera or a side
                    // frustum plane.  Do not hand QPainter an un-divided
                    // polygon with a negative-w vertex: its implicit edges
                    // can become a huge dashed diagonal.  Preserve the
                    // visible outline by clipping each 3D edge independently
                    // with the same homogeneous/frustum routine as the
                    // authored cleave line.  The fill is intentionally
                    // omitted for a partially visible polygon rather than
                    // extrapolated beyond the known clipped boundary.
                    painter.setPen(planePen);
                    painter.setBrush(Qt::NoBrush);
                    for (std::size_t edge = 0; edge < polygon3d.size(); ++edge)
                    {
                        const TVector& edgeFirst = polygon3d[edge];
                        const TVector& edgeSecond = polygon3d[(edge + 1) % polygon3d.size()];
                        const std::optional<render::frame_path::ProjectedSegment> projectedEdge =
                            render::frame_path::projectWorldSegment(
                                mvp,
                                edgeFirst,
                                edgeSecond,
                                width(),
                                height());
                        if (projectedEdge)
                        {
                            painter.drawLine(projectedEdge->first, projectedEdge->second);
                        }
                    }
                    continue;
                }

                painter.setPen(planePen);
                painter.setBrush(planeColor);
                painter.drawPolygon(polygon2d);

                const bool lineIsAttachedToPolygon = render::cleaveLineMatchesPolygonEdge(
                    line.first, line.second, polygon3d);

                // A same-colored leader ties an unattached authored line to
                // every other target-sector intersection of its infinite plane.
                if (haveLineMidpoint && !lineIsAttachedToPolygon)
                {
                    QPointF polygonCenter{};
                    for (const QPointF& point : polygon2d)
                    {
                        polygonCenter += point;
                    }
                    polygonCenter /= static_cast<qreal>(polygon2d.size());
                    const QPointF delta = polygonCenter - lineMidpoint;
                    if (delta.x() * delta.x() + delta.y() * delta.y() > 64.0)
                    {
                        QPen leaderPen(planeEdge);
                        leaderPen.setWidthF(selected ? 1.5 : 1.0);
                        leaderPen.setStyle(Qt::DotLine);
                        painter.setPen(leaderPen);
                        painter.setBrush(Qt::NoBrush);
                        painter.drawLine(lineMidpoint, polygonCenter);
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(planeEdge);
                        painter.drawEllipse(polygonCenter, selected ? 3.0 : 2.2, selected ? 3.0 : 2.2);
                    }
                }
            }
        };

        if (sectorCleaveToolEnabled_)
        {
            for (std::size_t index = 0; index < displayedCleaveLines.size(); ++index)
            {
                const bool selected = stagedCleaveLineSelected(
                    static_cast<std::int32_t>(index));
                drawSectorCleavePlane(
                    displayedCleaveLines[index],
                    stagedLineColor(index),
                    selected);
            }
        }

        for (std::size_t index = 0; index < displayedCleaveLines.size(); ++index)
        {
            if (static_cast<std::int32_t>(index) == editingStagedCleaveLine_)
            {
                continue;
            }
            const StagedCleaveLine& line = displayedCleaveLines[index];
            drawCleaveLine(
                line.first,
                line.second,
                stagedLineColor(index),
                stagedCleaveLineSelected(static_cast<std::int32_t>(index)),
                index,
                &line);
        }

        if (cleaveAnglesVisible_)
        {
            for (const render::CleaveJointAngle& joint : jointGuides.joints)
            {
                const auto vertex = render::frame_path::projectWorldPoint(mvp, joint.joint, width(), height());
                const auto firstEdge = render::frame_path::projectWorldSegment(
                    mvp, joint.joint, joint.firstOther, width(), height());
                const auto secondEdge = render::frame_path::projectWorldSegment(
                    mvp, joint.joint, joint.secondOther, width(), height());
                if (!vertex || !firstEdge || !secondEdge)
                {
                    continue;
                }
                QPointF firstDirection = firstEdge->second - *vertex;
                QPointF secondDirection = secondEdge->second - *vertex;
                const double firstLength = std::hypot(firstDirection.x(), firstDirection.y());
                const double secondLength = std::hypot(secondDirection.x(), secondDirection.y());
                if (firstLength <= 1.0e-6 || secondLength <= 1.0e-6)
                {
                    continue;
                }
                firstDirection /= firstLength;
                secondDirection /= secondLength;
                const double startAngle = std::atan2(firstDirection.y(), firstDirection.x());
                const double sweep = std::remainder(
                    std::atan2(secondDirection.y(), secondDirection.x()) - startAngle,
                    2.0 * 3.14159265358979323846);
                QPainterPath arc{};
                for (int step = 0; step <= 18; ++step)
                {
                    const double angle = startAngle + sweep * static_cast<double>(step) / 18.0;
                    const QPointF point = *vertex + QPointF(std::cos(angle) * 18.0, std::sin(angle) * 18.0);
                    if (step == 0) arc.moveTo(point);
                    else arc.lineTo(point);
                }
                const bool selected = stagedCleaveLineSelected(static_cast<std::int32_t>(joint.firstLine))
                    || stagedCleaveLineSelected(static_cast<std::int32_t>(joint.secondLine));
                const QColor color = selected ? QColor(255, 210, 72, 245) : stagedLineColor(joint.firstLine);
                painter.setPen(QPen(color, 1.25));
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(arc);
                const double middle = startAngle + sweep * 0.5;
                drawLabel(*vertex + QPointF(std::cos(middle) * 34.0, std::sin(middle) * 34.0),
                    render::compactCleaveMeasurementNumber(joint.degrees, 1) + QChar(0x00b0), color);
            }
        }

        if ( cleaveShapeOptions_.placementArmed
            && stagedCleaveShapePlacementPlan_
            && stagedCleaveShapePreviewFrame_ )
        {
            const StagedCleaveShapePlacementFrame& frame =
                *stagedCleaveShapePreviewFrame_;
            const auto mapShapePoint = [&frame](
                                           const ui::viewport::CleaveShapePoint2D& point)
            {
                return frame.center
                    + frame.axisA * point.x
                    + frame.axisB * point.y;
            };
            const QColor previewColor(138, 242, 190, 225);
            QPen previewPen(previewColor);
            previewPen.setWidthF(2.0);
            previewPen.setStyle(Qt::DashLine);
            previewPen.setCapStyle(Qt::RoundCap);
            painter.setPen(previewPen);
            painter.setBrush(Qt::NoBrush);
            const auto drawShapeStroke = [&] (
                                             const ui::viewport::CleaveShapeStroke2D& stroke)
            {
                const std::optional<render::frame_path::ProjectedSegment> segment =
                    render::frame_path::projectWorldSegment(
                        mvp,
                        mapShapePoint(stroke.first),
                        mapShapePoint(stroke.second),
                        width(),
                        height());
                if ( segment )
                {
                    painter.drawLine(segment->first, segment->second);
                }
            };
            for ( const ui::viewport::CleaveShapeStroke2D& stroke
                : stagedCleaveShapePlacementPlan_->decompositionStrokes )
            {
                drawShapeStroke(stroke);
            }
            const std::vector<ui::viewport::CleaveShapePoint2D>& perimeter =
                stagedCleaveShapePlacementPlan_->perimeter;
            for ( std::size_t index = 0; index < perimeter.size(); ++index )
            {
                drawShapeStroke(ui::viewport::CleaveShapeStroke2D{
                    perimeter[index],
                    perimeter[(index + 1u) % perimeter.size()]});
            }
            if ( const std::optional<QPointF> center =
                    render::frame_path::projectWorldPoint(
                        mvp,
                        frame.center,
                        width(),
                        height()) )
            {
                painter.setPen(QColor(238, 247, 250, 235));
                painter.drawText(
                    center->toPoint() + QPoint(9, -9),
                    QStringLiteral("Click to place"));
            }
        }

        auto newLinePreviewFrame = [&](const TVector& first,
                                       const TVector& second,
                                       const bool sectorTool)
            -> std::optional<StagedCleaveLine>
        {
            if (!cleaveStagedMode_ || editingStagedCleaveLine_ >= 0)
            {
                return std::nullopt;
            }

            TVector direction = second - first;
            if (!normalizeInPlace(direction))
            {
                return std::nullopt;
            }

            TVector planeNormal{};
            if (sectorTool)
            {
                if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
                {
                    TVector firstRay = first - render::cleaveDocumentPoint(eye);
                    TVector secondRay = second - render::cleaveDocumentPoint(eye);
                    if (!normalizeInPlace(firstRay) || !normalizeInPlace(secondRay))
                    {
                        return std::nullopt;
                    }
                    planeNormal = render::cleaveCross(firstRay, secondRay);
                }
                else
                {
                    TVector viewDirection = render::cleaveDocumentPoint(target) - render::cleaveDocumentPoint(eye);
                    if (!normalizeInPlace(viewDirection))
                    {
                        return std::nullopt;
                    }
                    planeNormal = render::cleaveCross(viewDirection, direction);
                }
            }
            else
            {
                if (!level_ || selectedSurfaceSector_ < 0 || selectedSurface_ < 0
                    || static_cast<std::size_t>(selectedSurfaceSector_) >= level_->sectors.size())
                {
                    return std::nullopt;
                }
                const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSurfaceSector_)];
                if (static_cast<std::size_t>(selectedSurface_) >= sector.surfaces.size())
                {
                    return std::nullopt;
                }
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(selectedSurface_)];
                TVector surfaceNormal(
                    surface.normal.x,
                    surface.normal.y,
                    surface.normal.z);
                if (!normalizeInPlace(surfaceNormal) && surface.vertices.size() >= 3)
                {
                    const std::int32_t firstIndex = surface.vertices[0];
                    const std::int32_t secondIndex = surface.vertices[1];
                    const std::int32_t thirdIndex = surface.vertices[2];
                    if (firstIndex < 0 || secondIndex < 0 || thirdIndex < 0
                        || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
                        || static_cast<std::size_t>(secondIndex) >= sector.vertices.size()
                        || static_cast<std::size_t>(thirdIndex) >= sector.vertices.size())
                    {
                        return std::nullopt;
                    }
                    const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(firstIndex)];
                    const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(secondIndex)];
                    const Vertex& thirdVertex = sector.vertices[static_cast<std::size_t>(thirdIndex)];
                    const TVector firstPoint(
                        firstVertex.x,
                        firstVertex.y,
                        firstVertex.z);
                    const TVector secondPoint(
                        secondVertex.x,
                        secondVertex.y,
                        secondVertex.z);
                    const TVector thirdPoint(
                        thirdVertex.x,
                        thirdVertex.y,
                        thirdVertex.z);
                    surfaceNormal = render::cleaveCross(
                        secondPoint - firstPoint,
                        thirdPoint - firstPoint);
                    if (!normalizeInPlace(surfaceNormal))
                    {
                        return std::nullopt;
                    }
                }
                planeNormal = render::cleaveCross(surfaceNormal, direction);
            }

            if (!normalizeInPlace(planeNormal))
            {
                return std::nullopt;
            }

            StagedCleaveLine frame{};
            frame.first = first;
            frame.second = second;
            frame.planeNormal = planeNormal;
            TVector preferredReference(view(0, 0), view(0, 1), view(0, 2));
            if (isStagedCleaveInteractionActive() && stagedCleaveContext_.valid)
            {
                preferredReference = TVector(
                    stagedCleaveContext_.view(0, 0),
                    stagedCleaveContext_.view(0, 1),
                    stagedCleaveContext_.view(0, 2));
            }
            if ( cleaveChainEnabled_ && !displayedCleaveLines.empty() )
            {
                const std::vector<StagedCleaveLine>& originLines =
                    stagedCleaveChainOrigin_ && !endpointPreviewLines.empty()
                        ? endpointPreviewLines : stagedCleaveLines_;
                preferredReference = stagedCleaveChainOriginIncomingDirection(originLines);
            }
            initializeStagedCleaveLineFrame(frame, preferredReference);
            frame.linkedToPrevious = cleaveChainEnabled_
                && !displayedCleaveLines.empty()
                && stagedCleaveChainOriginIsCanonicalTailSecond();
            return frame;
        };

        if (sectorCleaveToolEnabled_ && sectorCleaveHaveFirstPoint_)
        {
            const std::optional<std::size_t> editedIndex = editingStagedCleaveLine_ >= 0
                ? std::optional<std::size_t>(static_cast<std::size_t>(editingStagedCleaveLine_))
                : std::nullopt;
            const bool movingFirstEndpoint = editingStagedCleaveEndpoint_ == 0;
            const TVector first = movingFirstEndpoint
                ? sectorCleaveCurrentPoint_
                : sectorCleaveFirstPoint_;
            const TVector second = movingFirstEndpoint
                ? sectorCleaveFirstPoint_
                : sectorCleaveCurrentPoint_;
            const std::optional<StagedCleaveLine> previewFrame = newLinePreviewFrame(
                first,
                second,
                true);
            const std::size_t colorIndex = editedIndex.value_or(displayedCleaveLines.size());
            const QColor lineColor = stagedLineColor(colorIndex);
            if (previewFrame)
            {
                drawSectorCleavePlane(*previewFrame, lineColor, true);
            }
            const StagedCleaveLine* displayFrame = previewFrame
                ? &*previewFrame
                : (editedIndex && *editedIndex < displayedCleaveLines.size()
                    ? &displayedCleaveLines[*editedIndex]
                    : nullptr);
            drawCleaveLine(
                first,
                second,
                lineColor,
                editingStagedCleaveLine_ >= 0,
                editedIndex,
                displayFrame);
        }
        if (surfaceCleaveToolEnabled_ && surfaceCleaveHaveFirstPoint_)
        {
            const std::optional<std::size_t> editedIndex = editingStagedCleaveLine_ >= 0
                ? std::optional<std::size_t>(static_cast<std::size_t>(editingStagedCleaveLine_))
                : std::nullopt;
            const bool movingFirstEndpoint = editingStagedCleaveEndpoint_ == 0;
            const TVector first = movingFirstEndpoint
                ? surfaceCleaveCurrentPoint_
                : surfaceCleaveFirstPoint_;
            const TVector second = movingFirstEndpoint
                ? surfaceCleaveFirstPoint_
                : surfaceCleaveCurrentPoint_;
            const std::optional<StagedCleaveLine> previewFrame = newLinePreviewFrame(
                first,
                second,
                false);
            const std::size_t colorIndex = editedIndex.value_or(displayedCleaveLines.size());
            const StagedCleaveLine* displayFrame = previewFrame
                ? &*previewFrame
                : (editedIndex && *editedIndex < displayedCleaveLines.size()
                    ? &displayedCleaveLines[*editedIndex]
                    : nullptr);
            drawCleaveLine(
                first,
                second,
                stagedLineColor(colorIndex),
                editingStagedCleaveLine_ >= 0,
                editedIndex,
                displayFrame);
        }

        if ( cleaveChainEnabled_ && !displayedCleaveLines.empty() )
        {
            const bool hasExplicitOrigin = stagedCleaveChainOrigin_
                && stagedCleaveChainOrigin_->lineIndex >= 0
                && static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex)
                    < displayedCleaveLines.size()
                && (stagedCleaveChainOrigin_->endpoint == 0
                    || stagedCleaveChainOrigin_->endpoint == 1);
            const std::size_t originLine = hasExplicitOrigin
                ? static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex)
                : displayedCleaveLines.size() - 1;
            const std::int32_t originEndpoint = hasExplicitOrigin
                ? stagedCleaveChainOrigin_->endpoint
                : 1;
            if ( originEndpoint == 0 || originEndpoint == 1 )
            {
                const StagedCleaveLine& line = displayedCleaveLines[
                    originLine];
                const TVector& origin = originEndpoint == 0 ? line.first : line.second;
                const std::optional<QPointF> position = render::frame_path::projectWorldPoint(
                    mvp,
                    origin,
                    width(),
                    height());
                if ( position )
                {
                    QPen originPen(QColor(82, 239, 222, 245));
                    originPen.setWidthF(2.0);
                    painter.setPen(originPen);
                    painter.setBrush(QColor(14, 25, 31, 220));
                    painter.drawEllipse(*position, 8.0, 8.0);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(82, 239, 222, 245));
                    painter.drawEllipse(*position, 2.0, 2.0);
                }
            }
        }

        const auto drawCleaveSnapMarker = [&](
            const render::CleaveSnapCandidate& snapMarker,
            const bool emphasized)
        {
            QPen markerPen(QColor(255, 48, 48, emphasized ? 245 : 185));
            markerPen.setWidthF(emphasized ? 2.6 : 1.6);
            markerPen.setCapStyle(Qt::RoundCap);
            painter.setPen(markerPen);
            painter.setBrush(Qt::NoBrush);
            if (snapMarker.kind == render::CleaveSnapTargetKind::SurfaceCenter)
            {
                const std::optional<QPointF> center = render::frame_path::projectWorldPoint(
                    mvp,
                    snapMarker.position,
                    width(),
                    height());
                if (center)
                {
                    const qreal crossRadiusPx = emphasized ? 7.0 : 5.0;
                    painter.drawLine(
                        *center + QPointF(-crossRadiusPx, -crossRadiusPx),
                        *center + QPointF(crossRadiusPx, crossRadiusPx));
                    painter.drawLine(
                        *center + QPointF(-crossRadiusPx, crossRadiusPx),
                        *center + QPointF(crossRadiusPx, -crossRadiusPx));
                }
            }
            else if (snapMarker.kind == render::CleaveSnapTargetKind::EdgeMidpoint)
            {
                const std::optional<QPointF> first = render::frame_path::projectWorldPoint(
                    mvp,
                    snapMarker.edgeFirst,
                    width(),
                    height());
                const std::optional<QPointF> second = render::frame_path::projectWorldPoint(
                    mvp,
                    snapMarker.edgeSecond,
                    width(),
                    height());
                if (first && second)
                {
                    if (const auto marker = render::cleaveEdgeMidpointMarkerSegment(
                            *first,
                            *second,
                            emphasized ? 7.0 : 5.0))
                    {
                        painter.drawLine(marker->first, marker->second);
                    }
                }
            }
            else
            {
                const std::optional<QPointF> point = render::frame_path::projectWorldPoint(
                    mvp,
                    snapMarker.position,
                    width(),
                    height());
                if (point)
                {
                    if (snapMarker.kind
                        == render::CleaveSnapTargetKind::Vertex)
                    {
                        const qreal diamondRadiusPx = emphasized ? 5.0 : 4.0;
                        QPolygonF diamond{};
                        diamond << *point + QPointF(0.0, -diamondRadiusPx)
                                << *point + QPointF(diamondRadiusPx, 0.0)
                                << *point + QPointF(0.0, diamondRadiusPx)
                                << *point + QPointF(-diamondRadiusPx, 0.0);
                        painter.drawPolygon(diamond);
                    }
                    else
                    {
                        // Existing Multi-cut endpoints remain distinguishable
                        // from geometry vertices with a compact circular ring.
                        painter.drawEllipse(*point, 5.0, 5.0);
                    }
                }
            }
        };

        if (cleaveSnapGuidesVisible_)
        {
            const bool surfaceMode = surfaceCleaveToolEnabled_
                && surfaceSubMode_ == SurfaceSubMode::Face;
            const std::vector<render::CleaveSnapCandidate> guideMarkers =
                cleaveGeometrySnapTargets(mvp, surfaceMode);
            for (const render::CleaveSnapCandidate& guideMarker : guideMarkers)
            {
                drawCleaveSnapMarker(guideMarker, false);
            }
        }
        if (cleaveSnapMarker_)
        {
            drawCleaveSnapMarker(*cleaveSnapMarker_, true);
        }

        const QString transformHudText = stagedCleaveTransformHudText();
        if ( !transformHudText.isEmpty() )
        {
            painter.save();
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            QFont hudFont = painter.font();
            hudFont.setBold(true);
            hudFont.setPointSizeF(std::max(9.0, hudFont.pointSizeF()));
            painter.setFont(hudFont);

            const QFontMetricsF metrics(hudFont);
            QPointF anchor(width() * 0.5, height() * 0.5);
            if (stagedCleaveTransformHudAnchor_)
            {
                const ProjectedPoint projected = projectWorldToWidget(
                    mvp, *stagedCleaveTransformHudAnchor_, width(), height());
                if (projected.valid && projected.clipW > 0.0f)
                {
                    anchor = projected.pos;
                }
            }
            const auto pill = render::cleaveTransformHudBadgeRect(QRectF(rect()), anchor,
                QSizeF(metrics.horizontalAdvance(transformHudText), metrics.height()));
            if (pill)
            {
                stagedCleaveTransformHudRect_ = *pill;
                const ViewportBannerStyle& style = viewportBannerSettings_.styles[
                    static_cast<std::size_t>(stagedCleaveTransformHudBannerKind_)];
                const qreal opacity = stagedCleaveTransformHudPresentation_.opacityAt(
                    render::cleave::CleaveHudPresentation::Clock::now());
                ui::viewport::drawCleaveHudBadge(
                    painter,
                    *pill,
                    transformHudText,
                    style.textColor,
                    style.backgroundColor,
                    style.borderColor,
                    opacity);
            }
            painter.restore();
        }
    }

    if (!placementPreviewLines_.empty())
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        TVector cursorGuideCenter{};
        double cursorGuideRadius = 0.0;
        if (placementPreviewCursorGuide_)
        {
            for (const TVector& point : placementPreviewLines_)
            {
                cursorGuideCenter = cursorGuideCenter + point;
            }
            cursorGuideCenter.scale(1.0 / static_cast<double>(placementPreviewLines_.size()));

            for (const TVector& point : placementPreviewLines_)
            {
                cursorGuideRadius = std::max(cursorGuideRadius, (point - cursorGuideCenter).length());
            }
        }

        auto withAlphaScale = [](QColor color, const double scale) -> QColor
        {
            color.setAlpha(std::clamp(
                static_cast<int>(std::lround(static_cast<double>(color.alpha()) * scale)),
                0,
                255));
            return color;
        };

        QPen shadowPen(placementPreviewCursorGuide_
            ? QColor(245, 248, 255, 58)
            : QColor(0, 0, 0, 170));
        shadowPen.setWidthF(placementPreviewCursorGuide_ ? 5.0 : 4.0);
        shadowPen.setCapStyle(Qt::RoundCap);
        shadowPen.setJoinStyle(Qt::RoundJoin);

        QPen linePen(placementPreviewCursorGuide_
            ? QColor(245, 248, 255, 225)
            : (placementPreviewLinesInvalid_
                ? QColor(255, 80, 65, 235)
                : QColor(120, 210, 255, 235)));
        linePen.setWidthF(placementPreviewCursorGuide_ ? 1.25 : 2.0);
        linePen.setCapStyle(Qt::RoundCap);
        linePen.setJoinStyle(Qt::RoundJoin);
        linePen.setStyle(Qt::SolidLine);

        for (std::size_t i = 0; i + 1 < placementPreviewLines_.size(); i += 2)
        {
            // Placement previews include sector-join, surface-extrude, and
            // Add Sector wires.  Projecting their endpoints independently is
            // not safe when one endpoint crosses the camera/side frustum: the
            // perspective divide can produce a flashing diagonal across the
            // viewport.  Use the same seven-halfspace clip as cleave and Thing
            // frame overlays, then draw only the finite visible segment.
            const std::optional<render::frame_path::ProjectedSegment> projected =
                render::frame_path::projectWorldSegment(
                    mvp,
                    placementPreviewLines_[i],
                    placementPreviewLines_[i + 1],
                    width(),
                    height());
            if (!projected)
            {
                continue;
            }
            const QPointF& a = projected->first;
            const QPointF& b = projected->second;

            double fade = 1.0;
            if (placementPreviewCursorGuide_ && cursorGuideRadius > 0.0f)
            {
                const double dist = std::max(
                    (placementPreviewLines_[i] - cursorGuideCenter).length(),
                    (placementPreviewLines_[i + 1] - cursorGuideCenter).length());
                const double edge = std::clamp(static_cast<double>(dist / cursorGuideRadius), 0.0, 1.0);
                const double t = std::clamp((edge - 0.18) / 0.82, 0.0, 1.0);
                const double smooth = t * t * (3.0 - 2.0 * t);
                fade = 1.0 - smooth * 0.82;
            }

            painter.setPen(shadowPen);
            if (placementPreviewCursorGuide_)
            {
                shadowPen.setColor(withAlphaScale(QColor(245, 248, 255, 58), fade));
                painter.setPen(shadowPen);
            }
            painter.drawLine(a, b);
            if (placementPreviewCursorGuide_)
            {
                linePen.setColor(withAlphaScale(QColor(245, 248, 255, 225), fade));
            }
            painter.setPen(linePen);
            painter.drawLine(a, b);
            if (!placementPreviewCursorGuide_)
            {
                const QPointF delta = b - a;
                const double len2 = delta.x() * delta.x() + delta.y() * delta.y();
                if (len2 < 16.0)
                {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(0, 0, 0, 150));
                    painter.drawEllipse(a, 4.5, 4.5);
                    painter.setBrush(placementPreviewLinesInvalid_
                        ? QColor(255, 80, 65, 235)
                        : QColor(120, 210, 255, 235));
                    painter.drawEllipse(a, 2.8, 2.8);
                    painter.setBrush(Qt::NoBrush);
                }
            }
        }
    }

    drawToastOverlay();
}

} // namespace sed
