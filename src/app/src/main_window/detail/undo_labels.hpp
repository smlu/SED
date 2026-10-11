#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "sed/editor/selection.hpp"

namespace sed::ui::undo {

/**
 * Builds the short, user-facing label shown in the Edit history menu.
 *
 * History labels are intentionally generated at the operation boundary rather
 * than in Document: the document knows how to restore a state, but only the
 * caller knows which kind of entity was edited. The singular/plural form is
 * based on the number of affected entities, not on the number of undo records.
 * Operations use past-tense verbs (`Moved`, `Changed`, `Added`) because the
 * history row describes a completed command and the same text is shown by
 * both Undo/Redo actions and the timeline.
 */
inline std::string action(std::string_view singular, std::string_view plural,
                          const std::size_t count, std::string_view verb) {
  std::string label{};
  const std::string_view entity = count == 1u ? singular : plural;
  label.reserve(entity.size() + verb.size() + 2u);
  label.append(entity);
  label.append(": ");
  label.append(verb);
  return label;
}

inline std::string action(std::string_view entity, const std::size_t count,
                          std::string_view verb) {
  return action(entity,
                entity == "Thing"                ? "Things"
                : entity == "Sector"             ? "Sectors"
                : entity == "Surface"            ? "Surfaces"
                : entity == "Frame"              ? "Frames"
                : entity == "Vertex"             ? "Vertices"
                : entity == "Edge"               ? "Edges"
                : entity == "COG"                ? "COGs"
                : entity == "COG Script"         ? "COG Scripts"
                : entity == "Thing Frame"        ? "Thing Frames"
                : entity == "Baked Light"        ? "Baked Lights"
                                                 : entity,
                count, verb);
}

/**
 * Build an entity-prefixed description whose operation already contains a
 * property name, for example `Sector: Point light range changed`.  Keeping
 * the entity prefix separate from the property is important for light edits:
 * a point light belongs to a Sector and a dynamic light parameter belongs to
 * a Thing, rather than becoming a third pseudo-entity such as `Sector Point
 * Light` in the history UI.
 */
inline std::string propertyAction(std::string_view singularEntity,
                                  std::string_view pluralEntity,
                                  const std::size_t count,
                                  std::string_view operation) {
  return action(singularEntity, pluralEntity, count, operation);
}

// Keep parameter names useful in the history menu without allowing a malformed
// or user-provided name to make an unbounded label.  The names are escaped as
// ASCII so embedded quotes, backslashes, control bytes, and arbitrary UTF-8
// bytes cannot break the quoted form shown to the user.
inline constexpr std::size_t kMaxParameterLabelNameBytes = 64u;

inline std::string escapedParameterName(std::string_view name) {
  if (name.empty()) {
    return "<unnamed>";
  }

  constexpr char hexDigits[] = "0123456789ABCDEF";
  std::string escaped{};
  escaped.reserve(kMaxParameterLabelNameBytes);
  bool truncated = false;
  std::size_t completePrefixBytes = 0;

  for (const char character : name) {
    const unsigned char byte = static_cast<unsigned char>(character);
    const bool printable = byte >= 0x20u && byte <= 0x7eu;
    const std::size_t encodedSize =
        printable && byte != static_cast<unsigned char>('\'') &&
                byte != static_cast<unsigned char>('\\')
            ? 1u
        : printable ? 2u
                    : 4u;
    if (escaped.size() + encodedSize > kMaxParameterLabelNameBytes) {
      truncated = true;
      break;
    }

    if (printable && byte != static_cast<unsigned char>('\'') &&
        byte != static_cast<unsigned char>('\\')) {
      escaped.push_back(static_cast<char>(byte));
    } else if (printable) {
      escaped.push_back('\\');
      escaped.push_back(static_cast<char>(byte));
    } else {
      escaped.push_back('\\');
      escaped.push_back('x');
      escaped.push_back(hexDigits[(byte >> 4u) & 0x0fu]);
      escaped.push_back(hexDigits[byte & 0x0fu]);
    }
    // Reserve the truncation suffix at a complete escape boundary.
    if (escaped.size() <= kMaxParameterLabelNameBytes - 3u) {
      completePrefixBytes = escaped.size();
    }
  }

  if (truncated) {
    constexpr std::string_view suffix = "...";
    if (escaped.size() + suffix.size() > kMaxParameterLabelNameBytes) {
      escaped.resize(completePrefixBytes);
    }
    escaped.append(suffix);
  }
  return escaped;
}

/** Build an entity-prefixed label for a named Thing/template parameter. */
inline std::string parameterAction(std::string_view entity,
                                   const std::size_t count,
                                   std::string_view parameterName,
                                   std::string_view verb) {
  const std::string safeName = escapedParameterName(parameterName);
  std::string parameterVerb{};
  parameterVerb.reserve(8u + safeName.size() + verb.size());
  parameterVerb.append("Param '");
  parameterVerb.append(safeName);
  parameterVerb.append("' ");
  parameterVerb.append(verb);
  return action(entity, count, parameterVerb);
}

/** Build a readable label for one gesture that edits two named parameters. */
inline std::string parameterPairAction(std::string_view entity,
                                       const std::size_t count,
                                       std::string_view firstParameterName,
                                       std::string_view secondParameterName,
                                       std::string_view verb) {
  const std::string first = escapedParameterName(firstParameterName);
  const std::string second = escapedParameterName(secondParameterName);
  std::string parameterVerb{};
  parameterVerb.reserve(9u + first.size() + second.size() + verb.size());
  parameterVerb.append("Params '");
  parameterVerb.append(first);
  parameterVerb.append("' and '");
  parameterVerb.append(second);
  parameterVerb.append("' ");
  parameterVerb.append(verb);
  return action(entity, count, parameterVerb);
}

/** Build a named parameter label for the currently selected Thing set. */
inline std::string thingParameterAction(const editor::Selection &selection,
                                        std::string_view parameterName,
                                        std::string_view verb) {
  // A property row can be edited with no selection when it is addressed by its
  // explicit Thing index. Treat that path as the singular operation it is.
  const std::size_t count =
      selection.things.empty() ? 1u : selection.things.size();
  return parameterAction("Thing", count, parameterName, verb);
}

/** Return a label for a selection containing one entity kind, or Selection. */
inline std::string selectionAction(const editor::Selection &selection,
                                   std::string_view verb) {
  const std::size_t sectors = selection.sectors.size();
  const std::size_t surfaces = selection.surfaces.size();
  const std::size_t things = selection.things.size();
  const std::size_t lights = selection.lights.size();
  const std::size_t frames = selection.frames.size();
  const std::size_t cogs = selection.cogs.size();
  const std::size_t kinds =
      (sectors != 0u ? 1u : 0u) + (surfaces != 0u ? 1u : 0u) +
      (things != 0u ? 1u : 0u) + (lights != 0u ? 1u : 0u) +
      (frames != 0u ? 1u : 0u) + (cogs != 0u ? 1u : 0u);

  if (kinds == 1u) {
    if (sectors != 0u) {
      return action("Sector", sectors, verb);
    }
    if (surfaces != 0u) {
      return action("Surface", surfaces, verb);
    }
    if (things != 0u) {
      return action("Thing", things, verb);
    }
    if (lights != 0u) {
      // Selection::lights addresses the persisted level light array, which
      // is the baked-light collection.  Dynamic Thing/sector lights have
      // their owning Thing/Sector selection and are labelled at the mutation
      // site with those owners.
      return action("Baked Light", lights, verb);
    }
    if (frames != 0u) {
      return action("Thing Frame", frames, verb);
    }
    return action("COG", cogs, verb);
  }

  return action("Selection", "Selection",
                sectors + surfaces + things + lights + frames + cogs, verb);
}

/**
 * Rewrites a previously authored `Entity: Verb` label to the correct plural
 * form for the current selection. Property editors construct their rows before
 * a user can change the selection, so this small boundary normalization keeps
 * labels such as `Thing: Position changed` accurate for multi-selection edits
 * without changing any mutation or undo grouping semantics.
 */
inline std::string relabelForSelection(std::string_view label,
                                       const editor::Selection &selection) {
  const std::size_t separator = label.find(": ");
  if (separator == std::string_view::npos) {
    return std::string(label);
  }

  const std::string_view entity = label.substr(0, separator);
  std::string_view singular = entity;
  std::string_view plural = entity;
  std::size_t count = 0u;
  if (entity == "Sector" || entity == "Sectors") {
    count = selection.sectors.size();
    singular = "Sector";
    plural = "Sectors";
  } else if (entity == "Surface" || entity == "Surfaces") {
    count = selection.surfaces.size();
    singular = "Surface";
    plural = "Surfaces";
  } else if (entity == "Thing" || entity == "Things") {
    count = selection.things.size();
    singular = "Thing";
    plural = "Things";
  } else if (entity == "Baked Light" || entity == "Baked Lights") {
    count = selection.lights.size();
    singular = "Baked Light";
    plural = "Baked Lights";
  } else if (entity == "COG" || entity == "COGs") {
    count = selection.cogs.size();
    singular = "COG";
    plural = "COGs";
  } else if (entity == "Frame" || entity == "Frames" ||
             entity == "Thing Frame" || entity == "Thing Frames") {
    count = selection.frames.size();
    if (entity == "Frame" || entity == "Frames") {
      singular = "Frame";
      plural = "Frames";
    } else {
      singular = "Thing Frame";
      plural = "Thing Frames";
    }
  } else if (entity == "Vertex" || entity == "Vertices") {
    count = selection.surfaceVertices.size();
    singular = "Vertex";
    plural = "Vertices";
  }

  if (count == 0u) {
    return std::string(label);
  }

  return action(singular, plural, count, label.substr(separator + 2u));
}

} // namespace sed::ui::undo
