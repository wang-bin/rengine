#pragma once

RENGINE_BEGIN_NAMESPACE

struct TransformNode_rotateAroundX {
    void operator()(double rotation, TransformNode *node) {
        node->setMatrix(mat4::rotateAroundX(rotation));
    }
};

struct TransformNode_rotateAroundY {
    void operator()(double rotation, TransformNode *node) {
        node->setMatrix(mat4::rotateAroundY(rotation));
    }
};

struct TransformNode_rotateAroundZ {
    void operator()(double rotation, TransformNode *node) {
        node->setMatrix(mat4::rotateAroundZ(rotation));
    }
};

RENGINE_END_NAMESPACE