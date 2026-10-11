#include "main_window/common.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "ui/components/light_parameters_dialog.hpp"

namespace sed {
namespace {

using LightDialog = ui::LightParametersDialog;

bool setLocalThingValue(Thing &thing, const std::string_view name,
                        const std::string_view text) {
  const auto current = std::find_if(
      thing.values.begin(), thing.values.end(), [&](const TplValue &value) {
        return sed::text::iequalsAscii(value.name, name);
      });
  if (current != thing.values.end() && current->asString() == text) {
    return false;
  }
  (void)thing::light_authoring::upsertStringValue(thing, name, text);
  return true;
}

LightDialog::Values valuesForThingLight(const Level &level,
                                        const assets::Templates *templates,
                                        const Thing &thing,
                                        const bool headlight) {
  LightDialog::Values values{};
  const std::string_view intensityName = headlight ? "lightintensity" : "light";
  if (const TplValue *authored =
          level::thingFindValue(level, templates, thing, intensityName)) {
    if (level.kind == ProjectType::IJIM) {
      const render::lighting::IjimAuthoredLight parsed =
          render::lighting::parseIjimAuthoredLight(*authored);
      if (parsed.valid) {
        values.red = parsed.color.x();
        values.green = parsed.color.y();
        values.blue = parsed.color.z();
        values.range = parsed.range;
      }
    } else {
      values.intensity = render::lighting::scalarThingLightValue(*authored);
    }
  }

  if (headlight) {
    if (const TplValue *offset =
            level::thingFindValue(level, templates, thing, "lightoffset")) {
      const render::lighting::IjimAuthoredLight parsed =
          render::lighting::parseIjimAuthoredLight(*offset);
      if (parsed.valid) {
        values.offsetX = parsed.color.x();
        values.offsetY = parsed.color.y();
        values.offsetZ = parsed.color.z();
      }
    }
  }
  return values;
}

} // namespace

void MainWindow::openThingLightValueDialog(const std::int32_t thingIndex,
                                            const std::string_view valueName,
                                            QWidget *anchor) {
  if (!documentEditingAllowed()) {
    return;
  }
  Level &level = document_.level();
  if (thingIndex < 0 ||
      static_cast<std::size_t>(thingIndex) >= level.things.size()) {
    return;
  }

  const bool regular = text::iequalsAscii(valueName, "light");
  const bool headlight = text::iequalsAscii(valueName, "lightintensity") ||
                         text::iequalsAscii(valueName, "lightoffset");
  if (!regular && !headlight) {
    return;
  }

  const assets::Templates *templatesDb =
      templates_.size() > 0 ? &templates_ : nullptr;
  const Thing &source = level.things[static_cast<std::size_t>(thingIndex)];
  const std::string_view requiredValue = headlight ? "lightintensity" : "light";
  if (!level::thingFindValue(level, templatesDb, source, requiredValue)) {
    return;
  }

  std::vector<std::int32_t> targets{};
  if (!document_.selection().things.empty()) {
    targets.assign(document_.selection().things.begin(),
                   document_.selection().things.end());
  } else {
    targets.push_back(thingIndex);
  }

  const LightDialog::Mode mode =
      headlight
          ? (level.kind == ProjectType::IJIM
                 ? LightDialog::Mode::ijimHeadlight
                 : LightDialog::Mode::classicHeadlight)
          : (level.kind == ProjectType::IJIM ? LightDialog::Mode::ijimThing
                                             : LightDialog::Mode::classicThing);
  const LightDialog::Values initial =
      valuesForThingLight(level, templatesDb, source, headlight);
  // The popup edits authored parameters on the Thing.  A Headlight popup
  // changes both fields in one gesture, so name both explicitly instead of
  // presenting a slash-packed pseudo-parameter in the history.
  const std::string undoLabel = headlight
      ? ui::undo::parameterPairAction(
            "Thing", targets.size(), "lightintensity", "lightoffset",
            "changed")
      : ui::undo::parameterAction("Thing", targets.size(), valueName,
                                  "changed");

  const auto applyValues =
      [targets, headlight](editor::DocumentState &state,
                           const LightDialog::Values &values) -> bool {
    bool any = false;
    for (const std::int32_t index : targets) {
      if (index < 0 ||
          static_cast<std::size_t>(index) >= state.level.things.size()) {
        continue;
      }
      Thing &thing = state.level.things[static_cast<std::size_t>(index)];
      if (headlight) {
        const std::string intensityText =
            state.level.kind == ProjectType::IJIM
                ? thing::light_authoring::formatIjimLight(
                      values.red, values.green, values.blue, values.range)
                : thing::light_authoring::formatScalar(values.intensity);
        any = setLocalThingValue(thing, "lightintensity", intensityText) || any;
        any = setLocalThingValue(
                  thing, "lightoffset",
                  thing::light_authoring::formatVector(
                      values.offsetX, values.offsetY, values.offsetZ)) ||
              any;
      } else {
        const std::string lightText =
            state.level.kind == ProjectType::IJIM
                ? thing::light_authoring::formatIjimLight(
                      values.red, values.green, values.blue, values.range)
                : thing::light_authoring::formatScalar(values.intensity);
        any = setLocalThingValue(thing, "light", lightText) || any;
      }
    }
    return any;
  };

  const auto notify = [this]() {
    if (glView_) {
      glView_->notifyThingsChanged();
    }
  };

  // One popup interaction is one undo record. Document and viewport state
  // update for every sample, while the comparatively expensive property tree
  // and checker refresh once when the popup closes.
  const auto undoPushed = std::make_shared<bool>(false);
  const auto refreshProperties = [this]() {
    const int verticalScroll = properties_ && properties_->verticalScrollBar()
                                   ? properties_->verticalScrollBar()->value()
                                   : 0;
    const int horizontalScroll =
        properties_ && properties_->horizontalScrollBar()
            ? properties_->horizontalScrollBar()->value()
            : 0;
    rebuildProperties();
    if (properties_ && properties_->verticalScrollBar()) {
      properties_->verticalScrollBar()->setValue(verticalScroll);
    }
    if (properties_ && properties_->horizontalScrollBar()) {
      properties_->horizontalScrollBar()->setValue(horizontalScroll);
    }
  };
  auto *dialog = new LightDialog(
      this,
      headlight ? QStringLiteral("Headlight") : QStringLiteral("Thing light"),
      mode, initial,
      [this, applyValues, notify, undoPushed,
       undoLabel](const LightDialog::Values &values) {
        if (!*undoPushed) {
          document_.pushUndo(undoLabel, editor::DocumentDirty::Things);
          updateEditActions();
          *undoPushed = true;
        }
        if (applyValues(document_.current(), values)) {
          notify();
        }
      },
      [this, undoPushed, refreshProperties]() {
        if (*undoPushed) {
          scheduleRebuildChecks(editor::DocumentDirty::Things);
          refreshProperties();
        }
      });
  const QPoint popupPosition = anchor
      ? anchor->mapToGlobal(QPoint(anchor->width(), 0))
      : QCursor::pos();
  dialog->showAt(popupPosition);
}

void MainWindow::openBakedLightParametersDialog(const std::int32_t lightIndex) {
  if (!documentEditingAllowed()) {
    return;
  }
  Level &level = document_.level();
  if (lightIndex < 0 ||
      static_cast<std::size_t>(lightIndex) >= level.lights.size()) {
    return;
  }
  const SedLight &source = level.lights[static_cast<std::size_t>(lightIndex)];
  const bool supportsBakedColor = level.kind != ProjectType::JKDF2;
  LightDialog::Values initial{};
  initial.red = source.color.r;
  initial.green = source.color.g;
  initial.blue = source.color.b;
  initial.intensity = source.intensity;
  initial.range = source.range;

  std::vector<std::int32_t> targets{};
  if (!document_.selection().lights.empty()) {
    targets.assign(document_.selection().lights.begin(),
                   document_.selection().lights.end());
  } else {
    targets.push_back(lightIndex);
  }
  const auto applyValues =
      [targets, supportsBakedColor](editor::DocumentState &state,
                                    const LightDialog::Values &values) -> bool {
    bool any = false;
    for (const std::int32_t index : targets) {
      if (index < 0 ||
          static_cast<std::size_t>(index) >= state.level.lights.size()) {
        continue;
      }
      SedLight &light = state.level.lights[static_cast<std::size_t>(index)];
      ColorF nextColor = light.color;
      if (supportsBakedColor) {
        nextColor = ColorF{
            static_cast<float>(std::clamp(values.red, 0.0, 1.0)),
            static_cast<float>(std::clamp(values.green, 0.0, 1.0)),
            static_cast<float>(std::clamp(values.blue, 0.0, 1.0)),
            light.color.a,
        };
      }
      const double nextIntensity = std::max(0.0, values.intensity);
      const double nextRange = std::max(0.0, values.range);
      if (light.color.r == nextColor.r && light.color.g == nextColor.g &&
          light.color.b == nextColor.b && light.intensity == nextIntensity &&
          light.range == nextRange) {
        continue;
      }
      light.color = nextColor;
      light.intensity = nextIntensity;
      light.range = nextRange;
      any = true;
    }
    return any;
  };
  const auto notify = [this]() {
    if (glView_) {
      glView_->notifyLightsChanged();
    }
  };

  const std::string undoLabel = ui::undo::action(
      "Baked Light", targets.size(), "Color/intensity/range changed");
  const auto undoPushed = std::make_shared<bool>(false);
  const auto refreshProperties = [this]() {
    const int verticalScroll = properties_ && properties_->verticalScrollBar()
                                   ? properties_->verticalScrollBar()->value()
                                   : 0;
    const int horizontalScroll =
        properties_ && properties_->horizontalScrollBar()
            ? properties_->horizontalScrollBar()->value()
            : 0;
    rebuildProperties();
    if (properties_ && properties_->verticalScrollBar()) {
      properties_->verticalScrollBar()->setValue(verticalScroll);
    }
    if (properties_ && properties_->horizontalScrollBar()) {
      properties_->horizontalScrollBar()->setValue(horizontalScroll);
    }
  };
  auto *dialog = new LightDialog(
      this, QStringLiteral("Baked light"),
      supportsBakedColor ? LightDialog::Mode::baked
                         : LightDialog::Mode::jkdf2Baked,
      initial,
      [this, applyValues, notify, undoPushed,
       undoLabel](const LightDialog::Values &values) {
        if (!*undoPushed) {
          document_.pushUndo(undoLabel, editor::DocumentDirty::Lights);
          updateEditActions();
          *undoPushed = true;
        }
        if (applyValues(document_.current(), values)) {
          notify();
        }
      },
      [this, undoPushed, refreshProperties]() {
        if (*undoPushed) {
          scheduleRebuildChecks(editor::DocumentDirty::Lights);
          refreshProperties();
        }
      });
  dialog->showAt(QCursor::pos());
}

void MainWindow::openSectorLightParametersDialog(
    const std::int32_t sectorIndex) {
  if (!documentEditingAllowed()) {
    return;
  }
  Level &level = document_.level();
  if (level.kind != ProjectType::IJIM || sectorIndex < 0 ||
      static_cast<std::size_t>(sectorIndex) >= level.sectors.size()) {
    return;
  }
  const PointLight &source =
      level.sectors[static_cast<std::size_t>(sectorIndex)].pointLight;
  if (!pointLightHasAuthoredParameters(source)) {
    return;
  }
  LightDialog::Values initial{};
  initial.red = source.color.r;
  initial.green = source.color.g;
  initial.blue = source.color.b;
  initial.minRange = source.minRange;
  initial.maxRange = source.maxRange;

  std::vector<std::int32_t> targets{};
  if (!document_.selection().sectors.empty()) {
    targets.assign(document_.selection().sectors.begin(),
                   document_.selection().sectors.end());
  } else {
    targets.push_back(sectorIndex);
  }
  const auto applyValues =
      [targets](editor::DocumentState &state,
                const LightDialog::Values &values) -> bool {
    bool any = false;
    for (const std::int32_t index : targets) {
      if (index < 0 ||
          static_cast<std::size_t>(index) >= state.level.sectors.size()) {
        continue;
      }
      PointLight &light =
          state.level.sectors[static_cast<std::size_t>(index)].pointLight;
      const ColorF nextColor{
          static_cast<float>(std::max(0.0, values.red)),
          static_cast<float>(std::max(0.0, values.green)),
          static_cast<float>(std::max(0.0, values.blue)),
          light.color.a,
      };
      const double nextMin = std::max(0.0, values.minRange);
      const double nextMax = std::max(0.0, values.maxRange);
      if (light.color.r == nextColor.r && light.color.g == nextColor.g &&
          light.color.b == nextColor.b && light.minRange == nextMin &&
          light.maxRange == nextMax) {
        continue;
      }
      light.color = nextColor;
      light.minRange = nextMin;
      light.maxRange = nextMax;
      any = true;
    }
    return any;
  };
  const auto notify = [this]() {
    if (glView_) {
      glView_->notifyThingsChanged();
    }
  };

  const std::string undoLabel = ui::undo::propertyAction(
      "Sector", "Sectors", targets.size(),
      "Point light color, minimum range, and maximum range changed");
  const editor::DocumentDirty dirty =
      editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things;
  const auto undoPushed = std::make_shared<bool>(false);
  const auto refreshProperties = [this]() {
    const int verticalScroll = properties_ && properties_->verticalScrollBar()
                                   ? properties_->verticalScrollBar()->value()
                                   : 0;
    const int horizontalScroll =
        properties_ && properties_->horizontalScrollBar()
            ? properties_->horizontalScrollBar()->value()
            : 0;
    rebuildProperties();
    if (properties_ && properties_->verticalScrollBar()) {
      properties_->verticalScrollBar()->setValue(verticalScroll);
    }
    if (properties_ && properties_->horizontalScrollBar()) {
      properties_->horizontalScrollBar()->setValue(horizontalScroll);
    }
  };
  auto *dialog = new LightDialog(
      this, QStringLiteral("Sector point light"), LightDialog::Mode::ijimSector,
      initial,
      [this, applyValues, notify, undoPushed, dirty,
       undoLabel](const LightDialog::Values &values) {
        if (!*undoPushed) {
          document_.pushUndo(undoLabel, dirty);
          updateEditActions();
          *undoPushed = true;
        }
        if (applyValues(document_.current(), values)) {
          notify();
        }
      },
      [this, undoPushed, refreshProperties, dirty]() {
        if (*undoPushed) {
          scheduleRebuildChecks(dirty);
          refreshProperties();
        }
      });
  dialog->showAt(QCursor::pos());
}

} // namespace sed
