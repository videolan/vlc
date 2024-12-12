/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "qsgrectangularnoisenode.hpp"

#include <QSGMaterial>
#include <QSGGeometry>
#include <QSGMaterialShader>

// Sometimes culling can cause issues with the Qt renderer
#define BACKFACE_CULLING 1

class QSGRectangularNoiseMaterialShader : public QSGMaterialShader
{
public:
    QSGRectangularNoiseMaterialShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/Noise.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/Noise.frag.qsb"));

        // Indicate that we want to update the pipeline state, as we
        // want an extraordinary blending algorithm/factor:
        setFlag(QSGMaterialShader::UpdatesGraphicsPipelineState);
    }

    bool updateGraphicsPipelineState(QSGMaterialShader::RenderState &, QSGMaterialShader::GraphicsPipelineState *ps, QSGMaterial *, QSGMaterial *) override
    {
        /*  The default blending operations of the scene graph renderer are:
         *  QRhiGraphicsPipeline::BlendFactor srcColor = QRhiGraphicsPipeline::One;
         *  QRhiGraphicsPipeline::BlendFactor dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
         *  QRhiGraphicsPipeline::BlendFactor srcAlpha = QRhiGraphicsPipeline::One;
         *  QRhiGraphicsPipeline::BlendFactor dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
         *  QRhiGraphicsPipeline::BlendOp opColor = QRhiGraphicsPipeline::Add;
         *  QRhiGraphicsPipeline::BlendOp opAlpha = QRhiGraphicsPipeline::Add;
         *
         *  For the noise node, we want blend factors (One, One), instead of
         *  (One, OneMinusSrcAlpha). Here, we specify the factors that we
         *  want to use.
         */
        bool changeMade = false;

#if BACKFACE_CULLING
        if (Q_LIKELY(ps->cullMode != GraphicsPipelineState::CullBack))
        {
            // Culling is not necessary, but this rectangle will probably
            // not be rotated more than 90 degrees in z axis, so we can
            // have backface culling as optimization.
            ps->cullMode = GraphicsPipelineState::CullBack;
            changeMade = true;
        }
#endif

        if (Q_UNLIKELY(!ps->blendEnable)) // usually blending is enabled by other materials
        {
            ps->blendEnable = true;
            changeMade = true;
        }

        if (Q_UNLIKELY(ps->srcColor != GraphicsPipelineState::One))
        {
            ps->srcColor = GraphicsPipelineState::One;
            changeMade = true;
        }

        if (Q_LIKELY(ps->dstColor != GraphicsPipelineState::One))
        {
            ps->dstColor = GraphicsPipelineState::One;
            changeMade = true;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        // Qt 6.5.0 brings ability to use separate factor for alpha

        if (Q_UNLIKELY(ps->srcAlpha != GraphicsPipelineState::One))
        {
            ps->srcAlpha = GraphicsPipelineState::One;
            changeMade = true;
        }

        if (Q_LIKELY(ps->dstAlpha != GraphicsPipelineState::One))
        {
            ps->dstAlpha = GraphicsPipelineState::One;
            changeMade = true;
        }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // Before Qt 6.8.0, it is not possible to adjust the blend algorithm.
        // But it is not really important, because the default has always been addition.
        // And for the noise we want addition as well.

        if (Q_UNLIKELY(ps->opColor != GraphicsPipelineState::BlendOp::Add))
        {
            ps->opColor = GraphicsPipelineState::BlendOp::Add;
            changeMade = true;
        }

        if (Q_UNLIKELY(ps->opAlpha != GraphicsPipelineState::BlendOp::Add))
        {
            ps->opAlpha = GraphicsPipelineState::BlendOp::Add;
            changeMade = true;
        }
#endif

        return changeMade;
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        assert(dynamic_cast<QSGRectangularNoiseMaterial*>(newMaterial));
        assert(!oldMaterial || dynamic_cast<QSGRectangularNoiseMaterial*>(oldMaterial));

        qintptr offset = 0;
        QByteArray *buf = state.uniformData();

        // qt_Matrix:
        if (state.isMatrixDirty())
        {
            const QMatrix4x4 m = state.combinedMatrix();
            const size_t size = 64;
            memcpy(buf->data() + offset, m.constData(), size);
            offset += size;
        }

        // qt_Opacity:
        if (state.isOpacityDirty())
        {
            // For noise, maybe instead of the scene graph inherited opacity
            // provided here, direct opacity should be used?
            const float opacity = state.opacity();
            const size_t size = 4;
            memcpy(buf->data() + offset, &opacity, size);
            offset += size;
        }

        // Noise strength:
        const auto oldNoiseMaterial = static_cast<const QSGRectangularNoiseMaterial *>(oldMaterial);
        const auto newNoiseMaterial = static_cast<const QSGRectangularNoiseMaterial *>(newMaterial);
        if (!oldMaterial || (oldNoiseMaterial->strength() != newNoiseMaterial->strength()))
        {
            const float strength = newNoiseMaterial->strength();
            const size_t size = 4;
            memcpy(buf->data() + offset, &strength, size);
            offset += size;
        }

        return offset > 0;
    }
};

QSGRectangularNoiseMaterial::QSGRectangularNoiseMaterial()
{
    setFlag(QSGMaterial::Blending);
}

QSGMaterialType *QSGRectangularNoiseMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGRectangularNoiseMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new QSGRectangularNoiseMaterialShader;
}

int QSGRectangularNoiseMaterial::compare(const QSGMaterial *other) const
{
    assert(dynamic_cast<const QSGRectangularNoiseMaterial *>(other));
    return static_cast<const QSGRectangularNoiseMaterial*>(other)->strength() - strength();
}

QSGRectangularNoiseNode::QSGRectangularNoiseNode()
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
{
    QSGGeometry::updateRectGeometry(&m_geometry, QRectF());
    setMaterial(&m_material);
    setGeometry(&m_geometry);
}

void QSGRectangularNoiseNode::setRect(const QRectF &_rect)
{
    if (rect() == _rect)
        return;

    // We can use this instead of setting the geometry ourselves:
    QSGGeometry::updateRectGeometry(&m_geometry, _rect);
    markDirty(QSGNode::DirtyGeometry);
}

QRectF QSGRectangularNoiseNode::rect() const
{
    const QSGGeometry::Point2D *vertexData = m_geometry.vertexDataAsPoint2D();
    return QRectF(vertexData[0].x, vertexData[0].y,
                  vertexData[3].x - vertexData[0].x,
                  vertexData[3].y - vertexData[0].y);
}

float QSGRectangularNoiseNode::strength() const
{
    return m_material.strength();
}

void QSGRectangularNoiseNode::setStrength(float strength)
{
    if (qFuzzyCompare(m_material.strength(), strength))
        return;

    m_material.setStrength(strength);
    markDirty(QSGNode::DirtyMaterial);
}

