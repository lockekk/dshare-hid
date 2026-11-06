/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QSize>
#include <QString>

#include <optional>

namespace deskflow {

struct ComputerShape
{
  QString name;
  QSize size;
};

/// a client's size as the core tells the GUI, the name last because it's the only free text
inline QString computerShapeMessage(const QString &name, int width, int height)
{
  return QStringLiteral("%1,%2,%3").arg(width).arg(height).arg(name);
}

inline std::optional<ComputerShape> parseComputerShape(const QString &message)
{
  const auto widthEnd = message.indexOf(QLatin1Char(','));
  const auto heightEnd = message.indexOf(QLatin1Char(','), widthEnd + 1);
  if (widthEnd < 0 || heightEnd < 0)
    return std::nullopt;

  bool widthOk = false;
  bool heightOk = false;
  const int width = message.first(widthEnd).toInt(&widthOk);
  const int height = message.sliced(widthEnd + 1, heightEnd - widthEnd - 1).toInt(&heightOk);
  const auto name = message.sliced(heightEnd + 1);
  if (!widthOk || !heightOk || width <= 0 || height <= 0 || name.isEmpty())
    return std::nullopt;
  return ComputerShape{name, QSize(width, height)};
}

} // namespace deskflow
