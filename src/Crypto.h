#pragma once

#include <QByteArray>
#include <cstdint>

namespace crypto {

QByteArray generateKey(const QByteArray& data);
QByteArray makePacket(const QByteArray& key, int32_t seq, const QByteArray& data);

}
