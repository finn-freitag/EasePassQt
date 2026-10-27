#pragma once

#include <QString>
#include <QStringView>

namespace EasePass::Core {

/**
 * @brief Represents a common or predictable password sequence (keyboard walk, repeated chars,
 *        dictionary word, numeric run, etc.).
 *
 * C#: Models/CommonPasswordSequence.cs.
 *
 * Two modes:
 *  - Whole sequence (`CommonPasswordSequence(sequence)`): checks whether `sequence` occurs as a
 *    case-insensitive substring.
 *  - Sliding window (`CommonPasswordSequence(sequence, minLength)`): checks whether any contiguous
 *    subsequence of length `minLength` from `sequence` occurs case-insensitively in the password.
 *    If `sequence.length() <= minLength` (e.g. repeated character sequences like `"aaaaa"` with
 *    `minLength == 5`), the whole `sequence` is checked.
 */
class CommonPasswordSequence {
public:
    explicit CommonPasswordSequence(QString sequence)
        : m_sequence(std::move(sequence)), m_minLength(5), m_entireSequence(true) {}

    CommonPasswordSequence(QString sequence, int minLength)
        : m_sequence(std::move(sequence)), m_minLength(minLength), m_entireSequence(false) {}

    const QString &sequence() const { return m_sequence; }
    int minLength() const { return m_minLength; }
    bool isEntireSequence() const { return m_entireSequence; }

    bool containsSequence(QStringView str) const;

private:
    QString m_sequence;
    int m_minLength = 5;
    bool m_entireSequence = false;
};

} // namespace EasePass::Core
