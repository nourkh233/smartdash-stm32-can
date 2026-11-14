#include "framedecoder.h"
#include <QRegularExpression>

static quint16 u16(const QByteArray &d, int i) {
    return quint16(quint8(d.at(i)) | (quint8(d.at(i + 1)) << 8));   // little-endian
}
static qint16 i16(const QByteArray &d, int i) { return qint16(u16(d, i)); }

std::optional<FrameDecoder::Frame> FrameDecoder::parseLine(const QString &line) {
    const QString s = line.trimmed();
    const int sep = s.indexOf('#');
    if (sep < 1 || sep > 3) return std::nullopt;              // l'ID a 1 à 3 chiffres hexa

    bool ok = false;
    const uint id = s.left(sep).toUInt(&ok, 16);
    if (!ok || id > 0x7FF) return std::nullopt;               // ID standard sur 11 bits

    const QString hex = s.mid(sep + 1);
    static const QRegularExpression re("^[0-9A-Fa-f]*$");
    if (hex.size() % 2 != 0 || hex.size() > 16 || !re.match(hex).hasMatch())
        return std::nullopt;                                  // octets entiers, 8 au plus

    return Frame{ quint16(id), QByteArray::fromHex(hex.toLatin1()) };
}

QVector<Reading> FrameDecoder::decode(const Frame &f) {
    const QByteArray &d = f.data;
    switch (f.id) {
    case 0x110: if (d.size() == 2) return { Reading{Channel::Speed,    u16(d, 0) * 0.1} };  break;
    case 0x120: if (d.size() == 2) return { Reading{Channel::Distance, double(u16(d, 0))} }; break;
    case 0x130: if (d.size() == 2) return { Reading{Channel::Current,  i16(d, 0) * 0.01} }; break;
    case 0x140:
        if (d.size() == 2) return { Reading{Channel::Temperature, i16(d, 0) * 0.1} };
        if (d.size() == 4) return { Reading{Channel::Temperature, i16(d, 0) * 0.1},
                    Reading{Channel::Humidity,    u16(d, 2) * 0.1} };
        break;
    case 0x7F0: if (d.size() == 2) return { Reading{Channel::HbA, double(quint8(d.at(0)))} }; break;
    case 0x7F1: if (d.size() == 2) return { Reading{Channel::HbB, double(quint8(d.at(0)))} }; break;
    default: break;
    }
    return {};                                             // trame inconnue ou mal formée
}