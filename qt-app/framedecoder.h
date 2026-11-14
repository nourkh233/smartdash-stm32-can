#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <optional>

enum class Channel { Speed, Distance, Current, Temperature, Humidity, HbA, HbB, Count };

struct Reading { Channel ch; double value; };

namespace FrameDecoder {
struct Frame { quint16 id = 0; QByteArray data; };

std::optional<Frame> parseLine(const QString &line);   // "140#FD006202" -> Frame
QVector<Reading> decode(const Frame &f);               // vide si ID inconnu ou longueur fausse
}