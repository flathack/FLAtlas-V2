#include "BiniDecoder.h"
// TODO Phase 2

namespace flatlas::infrastructure {

bool BiniDecoder::isBini(const QByteArray &data)
{
    return data.size() >= 12 && data.startsWith("BINI");
}

QString BiniDecoder::decode(const QByteArray & /*data*/)
{
    // TODO Phase 2: Header, StringTable, Sections, Entries, Values parsen
    return {};
}

} // namespace flatlas::infrastructure
