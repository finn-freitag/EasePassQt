#include "CommonPasswordSequence.h"

namespace EasePass::Core {

bool CommonPasswordSequence::containsSequence(QStringView str) const {
    if (m_sequence.isEmpty() || str.isEmpty()) {
        return false;
    }

    const int seqLen = m_sequence.length();
    if (m_entireSequence || seqLen <= m_minLength || m_minLength <= 0) {
        return str.contains(QStringView(m_sequence), Qt::CaseInsensitive);
    }

    // C#: CommonPasswordSequence.ContainsSequence loops i < sequence.Length - minLength,
    // plus checks up to the final window of length minLength.
    const QStringView seqView(m_sequence);
    for (int i = 0; i <= seqLen - m_minLength; ++i) {
        if (str.contains(seqView.sliced(i, m_minLength), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

} // namespace EasePass::Core
