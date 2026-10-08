#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    enum class OracleType {
        Hazard,
        Gravity,
        Speed,
        Mode,
        Unknown
    };

    struct Prediction {
        OracleType type = OracleType::Unknown;
        float x = 0.f;
        int id = 0;
        float threat = 0.f;
    };

    const char* typeName(OracleType type) {
        switch (type) {
            case OracleType::Hazard:  return "HAZARD";
            case OracleType::Gravity: return "GRAVITY";
            case OracleType::Speed:   return "SPEED";
            case OracleType::Mode:    return "MODE";
            default:                  return "OBJECT";
        }
    }

    OracleType classify(GameObject* object) {
        const int id = object->m_objectID;

        // Common GD 2.2081 gameplay IDs.
        switch (id) {
            // Gravity portals.
            case 11:
            case 12:
            case 13:
            case 47:
            case 48:
                return OracleType::Gravity;

            // Game mode portals.
            case 95:
            case 96:
            case 97:
            case 98:
            case 99:
            case 101:
            case 111:
            case 133:
            case 142:
                return OracleType::Mode;

            // Speed portals.
            case 200:
            case 201:
            case 202:
            case 203:
                return OracleType::Speed;

            default:
                break;
        }

        // A conservative generic hazard check. Oracle does not need to
        // understand every decoration object to warn about dense geometry.
        if (object->m_objectID > 0 && object->m_objectID < 500) {
            return OracleType::Hazard;
        }

        return OracleType::Unknown;
    }

    float baseThreat(OracleType type) {
        switch (type) {
            case OracleType::Hazard:  return 1.0f;
            case OracleType::Gravity: return 0.75f;
            case OracleType::Speed:   return 0.70f;
            case OracleType::Mode:    return 0.65f;
            default:                  return 0.15f;
        }
    }
}

class $modify(OraclePlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* oracleLabel = nullptr;
        CCLabelBMFont* nextLabel = nullptr;
        float scanTimer = 0.f;
        bool initializedHUD = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;

        setupOracleHUD();
        return true;
    }

    void setupOracleHUD() {
        if (m_fields->initializedHUD)
            return;

        m_fields->oracleLabel = CCLabelBMFont::create("ORACLE: CALIBRATING", "bigFont.fnt");
        m_fields->oracleLabel->setScale(0.42f);
        m_fields->oracleLabel->setAnchorPoint({0.f, 0.5f});
        m_fields->oracleLabel->setPosition({12.f, CCDirector::sharedDirector()->getWinSize().height - 28.f});
        m_fields->oracleLabel->setZOrder(9999);
        this->addChild(m_fields->oracleLabel);

        m_fields->nextLabel = CCLabelBMFont::create("NEXT: scanning...", "bigFont.fnt");
        m_fields->nextLabel->setScale(0.32f);
        m_fields->nextLabel->setAnchorPoint({0.f, 0.5f});
        m_fields->nextLabel->setPosition({12.f, CCDirector::sharedDirector()->getWinSize().height - 48.f});
        m_fields->nextLabel->setZOrder(9999);
        this->addChild(m_fields->nextLabel);

        m_fields->initializedHUD = true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        m_fields->scanTimer += dt;
        if (m_fields->scanTimer < 0.08f)
            return;
        m_fields->scanTimer = 0.f;

        scanAhead();
    }

    void scanAhead() {
        if (!m_fields->oracleLabel || !m_player1 || !m_objects)
            return;

        const float playerX = m_player1->getPositionX();
        const float lookAhead = 950.f;

        float score = 0.f;
        float nearestX = FLT_MAX;
        Prediction nearest;

        int nearby = 0;
        int hazards = 0;

        for (auto* object : CCArrayExt<GameObject>(m_objects)) {
            if (!object || object->m_isDisabled)
                continue;

            const float dx = object->getPositionX() - playerX;
            if (dx < 0.f || dx > lookAhead)
                continue;

            ++nearby;

            const auto type = classify(object);
            if (type == OracleType::Unknown)
                continue;

            float threat = baseThreat(type);

            // The closer an object is, the more urgent it becomes.
            const float distanceFactor = 1.f - (dx / lookAhead);
            threat *= (0.35f + distanceFactor * 1.65f);

            if (type == OracleType::Hazard)
                ++hazards;

            score += threat;

            if (object->getPositionX() < nearestX) {
                nearestX = object->getPositionX();
                nearest = {type, object->getPositionX(), object->m_objectID, threat};
            }
        }

        // Cap the displayed score so dense decoration does not produce
        // meaningless numbers.
        score = std::min(score * 10.f, 100.f);

        const char* state = "SAFE";
        if (score >= 75.f)
            state = "YOU'RE COOKED";
        else if (score >= 45.f)
            state = "DANGER";
        else if (score >= 20.f)
            state = "CAUTION";

        m_fields->oracleLabel->setString(
            fmt::format("ORACLE: {}  {:02.0f}", state, score).c_str()
        );

        if (score >= 75.f)
            m_fields->oracleLabel->setColor({255, 70, 70});
        else if (score >= 45.f)
            m_fields->oracleLabel->setColor({255, 170, 60});
        else if (score >= 20.f)
            m_fields->oracleLabel->setColor({255, 235, 80});
        else
            m_fields->oracleLabel->setColor({120, 255, 150});

        if (nearestX != FLT_MAX) {
            const float distance = std::max(0.f, nearestX - playerX);
            m_fields->nextLabel->setString(
                fmt::format(
                    "NEXT: {}   {:.0f}px   |   {} objects",
                    typeName(nearest.type),
                    distance,
                    nearby
                ).c_str()
            );
        }
        else {
            m_fields->nextLabel->setString("NEXT: nothing detected");
        }

        // A very dense object field is worth flagging even when individual
        // object types are unknown. This is the Oracle's first anti-surprise
        // layer and deliberately avoids claiming perfect collision prediction.
        if (hazards >= 14 && score < 75.f) {
            m_fields->oracleLabel->setString(
                fmt::format("ORACLE: DANGER  {:02.0f}  [DENSE]", score).c_str()
            );
            m_fields->oracleLabel->setColor({255, 170, 60});
        }
    }
};
