/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyleBuilder.h"

#include "CSSFontSelector.h"
#include "CSSPaintImageValue.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "HTMLElement.h"
#include "PaintWorkletGlobalScope.h"
#include "Settings.h"
#include "StyleBuilderGenerated.h"
#include "StyleFontSizeFunctions.h"
#include "StylePropertyShorthand.h"

#include <wtf/SetForScope.h>

namespace WebCore {
namespace Style {

static const CSSPropertyID firstLowPriorityProperty = static_cast<CSSPropertyID>(lastHighPriorityProperty + 1);

inline PropertyCascade::Direction directionFromStyle(const RenderStyle& style)
{
    return { style.direction(), style.writingMode() };
}

inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyFill:
    case CSSPropertyStroke:
    case CSSPropertyStrokeColor:
        return true;
    default:
        break;
    }

    return false;
}

Builder::Builder(RenderStyle& style, BuilderContext&& context, const MatchResult& matchResult, CascadeLevel cascadeLevel, PropertyCascade::IncludedProperties includedProperties)
    : m_cascade(matchResult, cascadeLevel, includedProperties, directionFromStyle(style))
    , m_state(*this, style, WTFMove(context))
{
}

Builder::~Builder() = default;

void Builder::applyAllProperties()
{
    applyHighPriorityProperties();
    applyLowPriorityProperties();
}

// High priority properties may affect resolution of other properties (they are mostly font related).
void Builder::applyHighPriorityProperties()
{
    applyProperties(CSSPropertyWebkitRubyPosition, CSSPropertyWebkitRubyPosition);
    m_state.adjustStyleForInterCharacterRuby();

#if ENABLE(DARK_MODE_CSS)
    // Supported color schemes can affect resolved colors, so we need to apply that property before any color properties.
    applyProperties(CSSPropertyColorScheme, CSSPropertyColorScheme);
#endif

    applyProperties(firstCSSProperty, lastHighPriorityProperty);

    m_state.updateFont();
}

void Builder::applyLowPriorityProperties()
{
    ASSERT(!m_state.fontDirty());

    applyCustomProperties();
    applyProperties(firstLowPriorityProperty, lastCSSProperty);
    applyDeferredProperties();

    ASSERT(!m_state.fontDirty());
}

void Builder::applyDeferredProperties()
{
    for (auto& property : m_cascade.deferredProperties())
        applyCascadeProperty(property);
}

void Builder::applyProperties(int firstProperty, int lastProperty)
{
    if (LIKELY(m_cascade.customProperties().isEmpty()))
        return applyPropertiesImpl<CustomPropertyCycleTracking::Disabled>(firstProperty, lastProperty);

    return applyPropertiesImpl<CustomPropertyCycleTracking::Enabled>(firstProperty, lastProperty);
}

template<Builder::CustomPropertyCycleTracking trackCycles>
inline void Builder::applyPropertiesImpl(int firstProperty, int lastProperty)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!m_cascade.hasProperty(propertyID))
            continue;
        ASSERT(propertyID != CSSPropertyCustom);
        auto& property = m_cascade.property(propertyID);

        if (trackCycles == CustomPropertyCycleTracking::Enabled) {
            if (UNLIKELY(m_state.m_inProgressProperties.get(propertyID))) {
                // We are in a cycle (eg. setting font size using registered custom property value containing em).
                // So this value should be unset.
                m_state.m_appliedProperties.set(propertyID);
                // This property is in a cycle, and only the root of the call stack will have firstProperty != lastProperty.
                ASSERT(firstProperty == lastProperty);
                continue;
            }
            m_state.m_inProgressProperties.set(propertyID);
            applyCascadeProperty(property);
            m_state.m_appliedProperties.set(propertyID);
            m_state.m_inProgressProperties.set(propertyID, false);
            continue;
        }

        // If we don't have any custom properties, then there can't be any cycles.
        applyCascadeProperty(property);
    }
}

void Builder::applyCustomProperties()
{
    for (auto& name : m_cascade.customProperties().keys())
        applyCustomProperty(name);
}

void Builder::applyCustomProperty(const String& name)
{
    if (m_state.m_appliedCustomProperties.contains(name) || !m_cascade.customProperties().contains(name))
        return;

    auto property = m_cascade.customProperty(name);
    bool inCycle = m_state.m_inProgressPropertiesCustom.contains(name);

    SetForScope levelScope(m_state.m_currentProperty, &property);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && m_state.style().insideLink() == InsideLink::NotInside)
            continue;

        Ref valueToApply = downcast<CSSCustomPropertyValue>(*property.cssValue[index]);

        if (inCycle) {
            m_state.m_appliedCustomProperties.add(name); // Make sure we do not try to apply this property again while resolving it.
            valueToApply = CSSCustomPropertyValue::createWithID(name, CSSValueInvalid);
        }

        m_state.m_inProgressPropertiesCustom.add(name);

        if (std::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            RefPtr<CSSValue> parsedValue = resolvedVariableValue(CSSPropertyCustom, valueToApply.get());

            if (m_state.m_appliedCustomProperties.contains(name))
                return; // There was a cycle and the value was reset, so bail.

            if (!parsedValue)
                parsedValue = CSSCustomPropertyValue::createWithID(name, CSSValueUnset);

            valueToApply = downcast<CSSCustomPropertyValue>(*parsedValue);
        }

        if (m_state.m_inProgressPropertiesCustom.contains(name)) {
            SetForScope scopedLinkMatchMutation(m_state.m_linkMatch, index);
            applyProperty(CSSPropertyCustom, valueToApply.get(), index);
        }
    }

    m_state.m_inProgressPropertiesCustom.remove(name);
    m_state.m_appliedCustomProperties.add(name);

    for (auto index : { SelectorChecker::MatchDefault, SelectorChecker::MatchLink, SelectorChecker::MatchVisited }) {
        if (!property.cssValue[index])
            continue;
        if (index != SelectorChecker::MatchDefault && m_state.style().insideLink() == InsideLink::NotInside)
            continue;

        Ref valueToApply = downcast<CSSCustomPropertyValue>(*property.cssValue[index]);

        if (inCycle && std::holds_alternative<Ref<CSSVariableReferenceValue>>(valueToApply->value())) {
            // Resolve this value so that we reset its dependencies.
            resolvedVariableValue(CSSPropertyCustom, valueToApply.get());
        }
    }
}

inline void Builder::applyCascadeProperty(const PropertyCascade::Property& property)
{
    SetForScope levelScope(m_state.m_currentProperty, &property);

    auto applyWithLinkMatch = [&](SelectorChecker::LinkMatchMask linkMatch) {
        if (property.cssValue[linkMatch]) {
            SetForScope scopedLinkMatchMutation(m_state.m_linkMatch, linkMatch);
            applyProperty(property.id, *property.cssValue[linkMatch], linkMatch);
        }
    };

    applyWithLinkMatch(SelectorChecker::MatchDefault);

    if (m_state.style().insideLink() == InsideLink::NotInside)
        return;

    applyWithLinkMatch(SelectorChecker::MatchLink);
    applyWithLinkMatch(SelectorChecker::MatchVisited);

    m_state.m_linkMatch = SelectorChecker::MatchDefault;
}

void Builder::applyRollbackCascadeProperty(const PropertyCascade::Property& property, SelectorChecker::LinkMatchMask linkMatchMask)
{
    auto* value = property.cssValue[linkMatchMask];
    if (!value)
        return;

    SetForScope levelScope(m_state.m_currentProperty, &property);

    applyProperty(property.id, *value, linkMatchMask);
}

void Builder::applyProperty(CSSPropertyID id, CSSValue& value, SelectorChecker::LinkMatchMask linkMatchMask)
{
    ASSERT_WITH_MESSAGE(!isShorthandCSSProperty(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    auto valueToApply = resolveValue(id, value);

    if (CSSProperty::isDirectionAwareProperty(id)) {
        auto direction = m_cascade.direction();
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, direction.textDirection, direction.writingMode);
        ASSERT(newId != id);
        return applyProperty(newId, valueToApply.get(), linkMatchMask);
    }

    CSSCustomPropertyValue* customPropertyValue = nullptr;
    CSSValueID customPropertyValueID = CSSValueInvalid;
    CSSRegisteredCustomProperty* customPropertyRegistered = nullptr;

    if (id == CSSPropertyCustom) {
        customPropertyValue = downcast<CSSCustomPropertyValue>(valueToApply.ptr());
        ASSERT(customPropertyValue->isResolved());
        if (std::holds_alternative<CSSValueID>(customPropertyValue->value()))
            customPropertyValueID = std::get<CSSValueID>(customPropertyValue->value());
        auto& name = customPropertyValue->name();
        customPropertyRegistered = m_state.document().getCSSRegisteredCustomPropertySet().get(name);
    }

    bool isInherit = valueToApply->isInheritValue() || customPropertyValueID == CSSValueInherit;
    bool isInitial = valueToApply->isInitialValue() || customPropertyValueID == CSSValueInitial;

    bool isUnset = valueToApply->isUnsetValue() || customPropertyValueID == CSSValueUnset;
    bool isRevert = valueToApply->isRevertValue() || customPropertyValueID == CSSValueRevert;
    bool isRevertLayer = valueToApply->isRevertLayerValue() || customPropertyValueID == CSSValueRevertLayer;

    if (isRevert || isRevertLayer) {
        // In @keyframes, 'revert-layer' rolls back the cascaded value to the author level.
        // We can just not apply the property in order to keep the value from the base style.
        if (isRevertLayer && m_state.m_isBuildingKeyframeStyle)
            return;

        auto* rollbackCascade = isRevert ? ensureRollbackCascadeForRevert() : ensureRollbackCascadeForRevertLayer();

        if (rollbackCascade) {
            // With the rollback cascade built, we need to obtain the property and apply it. If the property is
            // not present, then we behave like "unset." Otherwise we apply the property instead of our own.
            if (customPropertyValue) {
                if (customPropertyRegistered && customPropertyRegistered->inherits && rollbackCascade->hasCustomProperty(customPropertyValue->name())) {
                    auto property = rollbackCascade->customProperty(customPropertyValue->name());
                    applyRollbackCascadeProperty(property, linkMatchMask);
                    return;
                }
            } else if (rollbackCascade->hasProperty(id)) {
                auto& property = rollbackCascade->property(id);
                applyRollbackCascadeProperty(property, linkMatchMask);
                return;
            } else if (rollbackCascade->hasDeferredProperty(id)) {
                auto& property = rollbackCascade->deferredProperty(id);
                applyRollbackCascadeProperty(property, linkMatchMask);
                return;
            }
        }

        isUnset = true;
    }

    if (isUnset) {
        if (CSSProperty::isInheritedProperty(id))
            isInherit = true;
        else
            isInitial = true;
    }

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit

    if (m_state.applyPropertyToVisitedLinkStyle() && !isValidVisitedLinkProperty(id)) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !CSSProperty::isInheritedProperty(id))
        m_state.style().setHasExplicitlyInheritedProperties();

#if ENABLE(CSS_PAINTING_API)
    if (is<CSSPaintImageValue>(valueToApply)) {
        auto& name = downcast<CSSPaintImageValue>(valueToApply.get()).name();
        if (auto* paintWorklet = const_cast<Document&>(m_state.document()).paintWorkletGlobalScopeForName(name)) {
            Locker locker { paintWorklet->paintDefinitionLock() };
            if (auto* registration = paintWorklet->paintDefinitionMap().get(name)) {
                for (auto& property : registration->inputProperties)
                    m_state.style().addCustomPaintWatchProperty(property);
            }
        }
    }
#endif

    BuilderGenerated::applyProperty(id, m_state, valueToApply.get(), isInitial, isInherit, customPropertyRegistered);
}

Ref<CSSValue> Builder::resolveValue(CSSPropertyID propertyID, CSSValue& value)
{
    if (!value.hasVariableReferences())
        return value;

    auto variableValue = resolvedVariableValue(propertyID, value);
    // If the cascade has already applied this id, then we detected a cycle, and this value should be unset.
    if (!variableValue || m_state.m_appliedProperties.get(propertyID)) {
        if (CSSProperty::isInheritedProperty(propertyID))
            return CSSValuePool::singleton().createValue(CSSValueInherit);
        return CSSValuePool::singleton().createValue(CSSValueInitial);
    }

    return *variableValue;
}

RefPtr<CSSValue> Builder::resolvedVariableValue(CSSPropertyID propertyID, const CSSValue& value)
{
    return CSSParser(m_state.document()).parseValueWithVariableReferences(propertyID, value, m_state);
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevert()
{
    auto rollbackCascadeLevel = m_state.m_currentProperty->cascadeLevel;
    if (rollbackCascadeLevel == CascadeLevel::UserAgent)
        return nullptr;

    --rollbackCascadeLevel;

    auto key = makeRollbackCascadeKey(rollbackCascadeLevel, 0);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, rollbackCascadeLevel);
    }).iterator->value.get();
}

const PropertyCascade* Builder::ensureRollbackCascadeForRevertLayer()
{
    auto& property = *m_state.m_currentProperty;
    auto rollbackLayerPriority = property.cascadeLayerPriority;
    if (!rollbackLayerPriority)
        return ensureRollbackCascadeForRevert();

    ASSERT(property.fromStyleAttribute == FromStyleAttribute::No || property.cascadeLayerPriority == RuleSet::cascadeLayerPriorityForUnlayered);

    // Style attribute reverts to the regular author style.
    if (property.fromStyleAttribute == FromStyleAttribute::No)
        --rollbackLayerPriority;

    auto key = makeRollbackCascadeKey(property.cascadeLevel, rollbackLayerPriority);
    return m_rollbackCascades.ensure(key, [&] {
        return makeUnique<const PropertyCascade>(m_cascade, property.cascadeLevel, rollbackLayerPriority);
    }).iterator->value.get();
}

auto Builder::makeRollbackCascadeKey(CascadeLevel cascadeLevel, CascadeLayerPriority cascadeLayerPriority) -> RollbackCascadeKey
{
    return { static_cast<unsigned>(cascadeLevel), static_cast<unsigned>(cascadeLayerPriority) };
}

}
}
