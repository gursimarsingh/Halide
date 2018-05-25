#ifndef HALIDE_HEXAGON_ALIGNMENT_H
#define HALIDE_HEXAGON_ALIGNMENT_H

/** \file
 * Class for analyzing Alignment of loads and stores for Hexagon.
 */

#include "Scope.h"
#include "ModulusRemainder.h"

namespace Halide {
namespace Internal {

enum class HexagonAlign {
    Unknown,     // We don't know if this load/store is aligned or unaligned.
    Aligned,     // We know the load/store is aligned to the required vector alignment.
    Unaligned,   // We know the load/store is not aligned to the required vector alignment.
};
class HexagonAlignmentAnalyzer {
    Scope<ModulusRemainder> alignment_info;
    int required_alignment;
public:
 HexagonAlignmentAnalyzer(int required_alignment, const Scope<ModulusRemainder>& alignment_info) :
    required_alignment(required_alignment) {
        this->alignment_info.set_containing_scope(&alignment_info);
    }
    /** Analyze the index of a load/store instruction for alignment
     *  It returns HexagonAlign::Unknown, HexagonAlign::Unaligned or HexagonAlign::Aligned
     */
    template<typename T>
    HexagonAlign is_aligned_impl(const T *op, int native_lanes, int *aligned_offset) {
        debug(3) << "HexagonAlignmentAnalyzer: Check if " << op << " is aligned to a "
                 << required_alignment << " byte boundary\n";
        debug(3) << "native_lanes: " << native_lanes << "\n";
        Expr index = op->index;
        const Ramp *ramp = index.as<Ramp>();
        const int64_t *const_stride = ramp ? as_const_int(ramp->stride) : nullptr;
        if (!ramp || !const_stride) {
            // We can't handle indirect loads, or loads with
            // non-constant strides.
            debug(3) << "Either not a ramp or not a constant stride, returning Unknown alignment.\n";
            return HexagonAlign::Unknown;
        }
        if (!(*const_stride == 1 || *const_stride == 2 || *const_stride == 3)) {
            debug(3) << "Can only deal with constant stride of 1, 2 or 3, returning Unknown alignment.\n";
            return HexagonAlign::Unknown;
        }

        // If this is a parameter, the base_alignment should be
        // host_alignment. Otherwise, this is an internal buffer,
        // which we assume has been aligned to the required alignment.
        int base_alignment =
            op->param.defined() ? op->param.host_alignment() : required_alignment;
        *aligned_offset = 0;
        bool known_alignment = false;
        if (base_alignment % required_alignment == 0) {
            // We know the base is aligned. Try to find out the offset
            // of the ramp base from an aligned offset.
            known_alignment = reduce_expr_modulo(ramp->base, native_lanes, aligned_offset,
                                                 alignment_info);
        }
        if (known_alignment) {
            debug(3) << "Is Aligned\n";
            return HexagonAlign::Aligned;
        }
        debug(3) << "Is Unaligned\n";
        return HexagonAlign::Unaligned;
    }
    HexagonAlign is_aligned(const Load *op, int *aligned_offset) {
        int native_lanes = required_alignment / op->type.bytes();
        return is_aligned_impl<Load>(op, native_lanes, aligned_offset);
    }
    HexagonAlign is_aligned(const Store *op, int *aligned_offset) {
        int native_lanes = required_alignment / op->value.type().bytes();
        return is_aligned_impl<Store>(op, native_lanes, aligned_offset);
    }

    Scope<ModulusRemainder>& get() { return alignment_info; }

    void push(const std::string &name, Expr v) {
        alignment_info.push(name, modulus_remainder(v, alignment_info));
    }
    void pop(const std::string &name) {
        alignment_info.pop(name);
    }
};

}  // namespace Internal
}  // namespace Halide
#endif
