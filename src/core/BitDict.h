// BitDict - maps bit positions to names, mirroring the reference BitDict behaviour:
// the array index is the bit position; duplicate names resolve to the first index.
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

namespace ctfak {

class BitDict {
public:
    BitDict() = default;
    BitDict(std::initializer_list<const char*> names, uint32_t flags = 0) : _flags(flags) {
        for (const char* n : names) _names.emplace_back(n);
    }

    void set(uint32_t flags) { _flags = flags; }
    uint32_t raw() const { return _flags; }

    bool operator[](size_t index) const { return (_flags & (1u << index)) != 0; }
    bool operator[](const std::string& name) const {
        for (size_t i = 0; i < _names.size(); ++i)
            if (_names[i] == name) return (_flags & (1u << i)) != 0;
        return false;
    }
    void setBit(const std::string& name, bool value) {
        for (size_t i = 0; i < _names.size(); ++i) {
            if (_names[i] == name) {
                if (value) _flags |= (1u << i);
                else _flags &= ~(1u << i);
                return;
            }
        }
    }
    const std::vector<std::string>& names() const { return _names; }

    std::string toString() const {
        std::ostringstream oss;
        bool first = true;
        for (size_t i = 0; i < _names.size(); ++i) {
            if ((_flags & (1u << i)) && !_names[i].empty() && _names[i][0] >= 'A') {
                if (!first) oss << ", ";
                oss << _names[i];
                first = false;
            }
        }
        return oss.str();
    }

private:
    std::vector<std::string> _names;
    uint32_t _flags = 0;
};

} // namespace ctfak
