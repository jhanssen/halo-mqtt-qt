#include "Crypto.h"
#include "qaesencryption.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace crypto {

QByteArray generateKey(const QByteArray& data)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(data);
    auto result = hash.result();
    std::reverse(result.begin(), result.end());
    return result.mid(0, 16);
}

QByteArray makePacket(const QByteArray& key, int32_t seq, const QByteArray& data)
{
    const uint8_t eof = 0xff;
    const uint16_t source = 0x8000;
    QByteArray iv(16, '\0');
    memcpy(iv.data(), &seq, 3);
    memcpy(iv.data() + 4, &source, 2);
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::OFB, QAESEncryption::ZERO);
    const auto payload = encryption.encode(data, key, iv);
    auto prehmac = QByteArray(13, '\0') + payload;
    memcpy(prehmac.data() + 8, &seq, 3);
    memcpy(prehmac.data() + 11, &source, 2);
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, key);
    hmac.addData(prehmac);
    auto hm = hmac.result();
    std::reverse(hm.begin(), hm.end());
    QByteArray out(14 + payload.size(), '\0');
    memcpy(out.data(), &seq, 3);
    memcpy(out.data() + 3, &source, 2);
    memcpy(out.data() + 5, payload.constData(), payload.size());
    memcpy(out.data() + 5 + payload.size(), hm.constData(), 8);
    memcpy(out.data() + 13 + payload.size(), &eof, 1);
    return out;
}

}
