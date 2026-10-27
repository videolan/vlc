/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#include "backdropblenditem.hpp"

#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGRendererInterface>

#include <cstring>

namespace {

class BackdropBlendMaterial : public QSGMaterial
{
public:
    BackdropBlendMaterial(QSGMaterialType *type, const QString& fragmentShader)
        : m_type(type)
        , m_fragmentShader(fragmentShader)
    {
        // This puts the node in the alpha pass, which is rendered after the
        // opaque pass and in back to front order. This is necessary so that
        // the content below is already rendered when this node is rendered:
        setFlag(QSGMaterial::Blending);
    }

    QSGMaterialType *type() const override
    {
        return m_type;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        const auto o = static_cast<const BackdropBlendMaterial *>(other);

        // The color is part of the pipeline state (blend constant):
        const QRgb a = m_color.rgba();
        const QRgb b = o->m_color.rgba();
        if (a != b)
            return (a < b) ? -1 : 1;

        if (m_uniforms.size() != o->m_uniforms.size())
            return (m_uniforms.size() < o->m_uniforms.size()) ? -1 : 1;

        for (qsizetype i = 0; i < m_uniforms.size(); ++i)
        {
            if (m_uniforms[i] != o->m_uniforms[i])
                return (m_uniforms[i] < o->m_uniforms[i]) ? -1 : 1;
        }

        return 0;
    }

    QSGMaterialType *const m_type;
    const QString m_fragmentShader;

    QColor m_color {Qt::transparent};
    QVector<float> m_uniforms;
};

class BackdropBlendMaterialShader : public QSGMaterialShader
{
public:
    explicit BackdropBlendMaterialShader(const QString& fragmentShader)
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/BackdropBlend.vert.qsb"));
        setShaderFileName(FragmentStage, fragmentShader);
        setFlag(UpdatesGraphicsPipelineState);
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        const auto material = static_cast<BackdropBlendMaterial *>(newMaterial);
        const auto old = static_cast<BackdropBlendMaterial *>(oldMaterial);

        QByteArray *buf = state.uniformData();

        bool changed = false;

        if (state.isMatrixDirty())
        {
            const QMatrix4x4 m = state.combinedMatrix();
            assert(buf->size() >= 64);
            memcpy(buf->data(), m.constData(), 64);
            changed = true;
        }

        if (state.isOpacityDirty())
        {
            const float opacity = state.opacity();
            assert(buf->size() >= 68);
            memcpy(buf->data() + 64, &opacity, 4);
            changed = true;
        }

        if (!old || old->m_uniforms != material->m_uniforms)
        {
            // Consecutive floats are tightly packed with std140:
            const qsizetype size = material->m_uniforms.size() * sizeof(float);
            assert(buf->size() >= 68 + size);
            memcpy(buf->data() + 68, material->m_uniforms.constData(), size);
            changed = true;
        }

        return changed;
    }

    bool updateGraphicsPipelineState(RenderState &, GraphicsPipelineState *ps,
                                     QSGMaterial *newMaterial, QSGMaterial *) override
    {
        const auto material = static_cast<BackdropBlendMaterial *>(newMaterial);

        bool changeMade = false;

        // `dst = constant * src + dst * (1 - src)`, where the fragment shader
        // outputs the same value (coverage) in all the channels. Using the
        // same factors for the alpha channel is intended:
        if (!ps->blendEnable)
        {
            ps->blendEnable = true;
            changeMade = true;
        }

        if (Q_LIKELY(ps->srcColor != GraphicsPipelineState::ConstantColor))
        {
            ps->srcColor = GraphicsPipelineState::ConstantColor;
            changeMade = true;
        }

        if (Q_LIKELY(ps->dstColor != GraphicsPipelineState::OneMinusSrcColor))
        {
            ps->dstColor = GraphicsPipelineState::OneMinusSrcColor;
            changeMade = true;
        }

        const QColor color = material->m_color.toRgb();
        const float alpha = color.alphaF();
        const QColor blendConstant = QColor::fromRgbF(color.redF() * alpha,
                                                      color.greenF() * alpha,
                                                      color.blueF() * alpha,
                                                      alpha);
        if (Q_LIKELY(ps->blendConstant != blendConstant))
        {
            ps->blendConstant = blendConstant;
            changeMade = true;
        }

        return changeMade;
    }
};

QSGMaterialShader *BackdropBlendMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new BackdropBlendMaterialShader(m_fragmentShader);
}

} // anonymous namespace

BackdropBlendItem::BackdropBlendItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents);
    connect(this, &QQuickItem::windowChanged, this, &BackdropBlendItem::updateSupported);

    connect(this, &QQuickItem::widthChanged, this, &BackdropBlendItem::markDirtyGeometry);
    connect(this, &QQuickItem::heightChanged, this, &BackdropBlendItem::markDirtyGeometry);
}

void BackdropBlendItem::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}

void BackdropBlendItem::updateSupported()
{
    const QQuickWindow* const w = window();
    const bool supported = w && w->rendererInterface() &&
                           QSGRendererInterface::isApiRhiBased(w->rendererInterface()->graphicsApi());

    if (m_supported == supported)
        return;
    m_supported = supported;
    emit supportedChanged();
}

QSGNode *BackdropBlendItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto node = static_cast<QSGGeometryNode *>(oldNode);

    const QRectF rect = boundingRect();
    const QRectF text = QRectF(0.0, 0.0, 1.0, 1.0);

    if (!m_supported || rect.isEmpty())
    {
        delete node;
        return nullptr;
    }

    if (!node)
    {
        node = new QSGGeometryNode;

        auto geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);

        node->setMaterial(new BackdropBlendMaterial(materialType(), fragmentShader()));
        node->setFlag(QSGNode::OwnsMaterial);
    }

    // The vertex position may be transformed by the renderer when batching,
    // so the position in item coordinates is also passed as texture coordinate:
    if (m_dirtyGeometry)
    {
        QSGGeometry::updateTexturedRectGeometry(node->geometry(), rect, text);
        node->geometry()->markVertexDataDirty();
        node->markDirty(QSGNode::DirtyGeometry);
        m_dirtyGeometry = false;
    }

    const auto material = static_cast<BackdropBlendMaterial *>(node->material());
    QVector<float> values = uniforms();
    if (material->m_color != m_color || material->m_uniforms != values)
    {
        material->m_color = m_color;
        material->m_uniforms = std::move(values);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    return node;
}
