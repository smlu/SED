#include "resource_picker_kind_utils.hpp"

#include "sed/level.hpp"

namespace sed::ui
{

std::optional<ResourcePickerKind> resourcePickerKindForCogType(const CogType type) noexcept
{
    switch (type)
    {
        case CogType::ai:        return ResourcePickerKind::ai;
        case CogType::cog:       return ResourcePickerKind::cog;
        case CogType::key:       return ResourcePickerKind::key;
        case CogType::mat:       return ResourcePickerKind::material;
        case CogType::msg:       return ResourcePickerKind::message;
        case CogType::model3do:  return ResourcePickerKind::model3do;
        case CogType::wav:       return ResourcePickerKind::wav;
        case CogType::template_: return ResourcePickerKind::template_;
        default:                 return std::nullopt;
    }
}

bool cogTypeAllowsDoubleClickAfterParseFailure(const CogType type) noexcept
{
    switch (type)
    {
        case CogType::cog:
        case CogType::ai:
        case CogType::key:
        case CogType::mat:
        case CogType::model3do:
        case CogType::wav:
        case CogType::template_:
            return true;
        default:
            return false;
    }
}

bool cogTypeUsesDoubleClickResourcePicker(const CogType type) noexcept
{
    switch (type)
    {
        case CogType::ai:
        case CogType::key:
        case CogType::mat:
        case CogType::model3do:
        case CogType::wav:
        case CogType::template_:
            return true;
        default:
            return false;
    }
}

bool cogTypeNeedsDoubleClickLineEditFallback(const CogType type) noexcept
{
    switch (type)
    {
        case CogType::sec:
        case CogType::surface:
        case CogType::thing:
        case CogType::cog:
            return true;
        default:
            return resourcePickerKindForCogType(type).has_value();
    }
}

std::optional<ResourcePickerKind> resourcePickerKindForAdjType(const AdjType type) noexcept
{
    switch (type)
    {
        case AdjType::mat:      return ResourcePickerKind::material;
        case AdjType::cog:      return ResourcePickerKind::cog;
        case AdjType::snd:      return ResourcePickerKind::snd;
        case AdjType::pup:      return ResourcePickerKind::pup;
        case AdjType::spr:      return ResourcePickerKind::spr;
        case AdjType::par:      return ResourcePickerKind::par;
        case AdjType::model3do: return ResourcePickerKind::model3do;
        case AdjType::ai:       return ResourcePickerKind::ai;
        case AdjType::tpl:      return ResourcePickerKind::template_;
        default:                return std::nullopt;
    }
}

std::optional<std::string_view> resourcePickerCategoryFileName(
    const ResourcePickerKind kind,
    const ProjectType project) noexcept
{
    switch (kind)
    {
        case ResourcePickerKind::material:
            switch (project)
            {
                case ProjectType::JKDF2: return "mats.jlh";
                case ProjectType::MOTS:  return "mmats.jlh";
                case ProjectType::IJIM:  return "ijimats.jlh";
                default:                 return std::nullopt;
            }
        case ResourcePickerKind::wav:
            switch (project)
            {
                case ProjectType::JKDF2: return "wavs.jlh";
                case ProjectType::MOTS:  return "mwavs.jlh";
                case ProjectType::IJIM:  return "ijimwavs.jlh";
                default:                 return std::nullopt;
            }
        case ResourcePickerKind::template_:
            switch (project)
            {
                case ProjectType::JKDF2: return "tpls.jlh";
                case ProjectType::MOTS:  return "mtpls.jlh";
                case ProjectType::IJIM:  return "ijimtpls.jlh";
                default:                 return std::nullopt;
            }
        default:
            return std::nullopt;
    }
}

} // namespace sed::ui
