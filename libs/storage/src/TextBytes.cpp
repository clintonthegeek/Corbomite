// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/TextBytes.h"

namespace Corbomite {

QByteArray stripUtf8Bom(QByteArray bytes)
{
    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes[0]) == 0xEF
        && static_cast<unsigned char>(bytes[1]) == 0xBB
        && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.remove(0, 3);
    }
    return bytes;
}

} // namespace Corbomite
