#include "game/presentation/hands/HandPresenceAnimator.hpp"

#include <cassert>

int main() {
    using ::biofuel::game::presentation::hands::HandPresenceAnimator;

    HandPresenceAnimator animator;
    assert(!animator.visible());
    assert(animator.alpha() == 0.0f);

    animator.update(true, 1.0f, false, 0.125f);
    assert(animator.visible());
    assert(animator.alpha() > 0.0f && animator.alpha() < 1.0f);
    assert(animator.scale() > 0.92f && animator.scale() < 1.0f);

    animator.update(true, 1.0f, false, 1.0f);
    assert(animator.alpha() > 0.0f && animator.alpha() < 1.0f);

    for (int i = 0; i < 8; ++i) {
        animator.update(true, 1.0f, false, 0.1f);
    }
    assert(animator.alpha() == 1.0f);
    assert(animator.scale() == 1.0f);

    for (int i = 0; i < 4; ++i) {
        animator.update(false, 0.0f, false, 0.1f);
    }
    assert(!animator.visible());
    assert(animator.alpha() == 0.0f);

    animator.update(false, 0.0f, true, 0.1f);
    assert(animator.visible());
    assert(animator.alpha() > 0.0f);

    animator.reset();
    assert(!animator.visible());
    assert(animator.alpha() == 0.0f);

    return 0;
}
