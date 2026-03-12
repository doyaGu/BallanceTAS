#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>

#include <yyjson.h>

#include "Logger.h"
#include "GameInterface.h"
#include "SavestateManager.h"
#include "ServiceContainer.h"
#include "physics_RT.h"

namespace fs = std::filesystem;

// ============================================================================
// SavestateData Serialization
// ============================================================================

string SavestateData::ToJson() const {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    // Metadata
    yyjson_mut_obj_add_str(doc, root, "name", name.c_str());
    yyjson_mut_obj_add_str(doc, root, "timestamp", timestamp.c_str());
    yyjson_mut_obj_add_str(doc, root, "levelName", levelName.c_str());
    yyjson_mut_obj_add_int(doc, root, "levelNumber", levelNumber);
    yyjson_mut_obj_add_str(doc, root, "description", description.c_str());

    // Physics state
    yyjson_mut_val *pos_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_real(doc, pos_obj, "x", position.x);
    yyjson_mut_obj_add_real(doc, pos_obj, "y", position.y);
    yyjson_mut_obj_add_real(doc, pos_obj, "z", position.z);
    yyjson_mut_obj_add_val(doc, root, "position", pos_obj);

    yyjson_mut_val *vel_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_real(doc, vel_obj, "x", velocity.x);
    yyjson_mut_obj_add_real(doc, vel_obj, "y", velocity.y);
    yyjson_mut_obj_add_real(doc, vel_obj, "z", velocity.z);
    yyjson_mut_obj_add_val(doc, root, "velocity", vel_obj);

    yyjson_mut_val *angVel_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_real(doc, angVel_obj, "x", angularVelocity.x);
    yyjson_mut_obj_add_real(doc, angVel_obj, "y", angularVelocity.y);
    yyjson_mut_obj_add_real(doc, angVel_obj, "z", angularVelocity.z);
    yyjson_mut_obj_add_val(doc, root, "angularVelocity", angVel_obj);

    yyjson_mut_val *rot_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_real(doc, rot_obj, "x", rotation.x);
    yyjson_mut_obj_add_real(doc, rot_obj, "y", rotation.y);
    yyjson_mut_obj_add_real(doc, rot_obj, "z", rotation.z);
    yyjson_mut_obj_add_real(doc, rot_obj, "w", rotation.w);
    yyjson_mut_obj_add_val(doc, root, "rotation", rot_obj);

    // RNG state
    yyjson_mut_val *rng_arr = yyjson_mut_arr(doc);
    for (uint32_t val : rngState) {
        yyjson_mut_arr_add_uint(doc, rng_arr, val);
    }
    yyjson_mut_obj_add_val(doc, root, "rngState", rng_arr);

    // Game state
    yyjson_mut_obj_add_int(doc, root, "points", points);
    yyjson_mut_obj_add_int(doc, root, "lives", lives);
    yyjson_mut_obj_add_int(doc, root, "sector", sector);
    yyjson_mut_obj_add_real(doc, root, "srScore", srScore);
    yyjson_mut_obj_add_real(doc, root, "hsScore", hsScore);

    // Time state
    yyjson_mut_obj_add_uint(doc, root, "tick", static_cast<uint64_t>(tick));

    // Write to string
    const char *json_str = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, nullptr);
    string result(json_str);
    free((void *) json_str);
    yyjson_mut_doc_free(doc);

    return result;
}

Result<SavestateData> SavestateData::FromJson(const string &json) {
    yyjson_doc *doc = yyjson_read(json.c_str(), json.length(), 0);
    if (!doc) {
        return Result<SavestateData>::Error("Failed to parse JSON");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return Result<SavestateData>::Error("JSON root is not an object");
    }

    SavestateData data;

    // Helper to get string
    auto get_str = [&](yyjson_val *object, const char *key) -> string {
        yyjson_val *val = object ? yyjson_obj_get(object, key) : nullptr;
        if (val && yyjson_is_str(val)) {
            return string(yyjson_get_str(val));
        }
        return "";
    };

    // Helper to get int
    auto get_int = [&](yyjson_val *object, const char *key, int default_val = 0) -> int {
        yyjson_val *val = object ? yyjson_obj_get(object, key) : nullptr;
        if (val && yyjson_is_num(val)) {
            return yyjson_get_int(val);
        }
        return default_val;
    };

    // Helper to get float
    auto get_real = [&](yyjson_val *object, const char *key, float default_val = 0.0f) -> float {
        yyjson_val *val = object ? yyjson_obj_get(object, key) : nullptr;
        if (val && yyjson_is_num(val)) {
            return static_cast<float>(yyjson_get_real(val));
        }
        return default_val;
    };

    // Metadata
    data.name = get_str(root, "name");
    data.timestamp = get_str(root, "timestamp");
    data.levelName = get_str(root, "levelName");
    data.levelNumber = get_int(root, "levelNumber");
    data.description = get_str(root, "description");

    // Physics state
    yyjson_val *pos_obj = yyjson_obj_get(root, "position");
    if (pos_obj && yyjson_is_obj(pos_obj)) {
        data.position.x = get_real(pos_obj, "x");
        data.position.y = get_real(pos_obj, "y");
        data.position.z = get_real(pos_obj, "z");
    }

    yyjson_val *vel_obj = yyjson_obj_get(root, "velocity");
    if (vel_obj && yyjson_is_obj(vel_obj)) {
        data.velocity.x = get_real(vel_obj, "x");
        data.velocity.y = get_real(vel_obj, "y");
        data.velocity.z = get_real(vel_obj, "z");
    }

    yyjson_val *angVel_obj = yyjson_obj_get(root, "angularVelocity");
    if (angVel_obj && yyjson_is_obj(angVel_obj)) {
        data.angularVelocity.x = get_real(angVel_obj, "x");
        data.angularVelocity.y = get_real(angVel_obj, "y");
        data.angularVelocity.z = get_real(angVel_obj, "z");
    }

    yyjson_val *rot_obj = yyjson_obj_get(root, "rotation");
    if (rot_obj && yyjson_is_obj(rot_obj)) {
        data.rotation.x = get_real(rot_obj, "x");
        data.rotation.y = get_real(rot_obj, "y");
        data.rotation.z = get_real(rot_obj, "z");
        data.rotation.w = get_real(rot_obj, "w", 1.0f);
    }

    // RNG state
    yyjson_val *rng_arr = yyjson_obj_get(root, "rngState");
    if (rng_arr && yyjson_is_arr(rng_arr)) {
        size_t idx, max;
        yyjson_val *val;
        yyjson_arr_foreach(rng_arr, idx, max, val) {
            if (yyjson_is_uint(val)) {
                data.rngState.push_back(static_cast<uint32_t>(yyjson_get_uint(val)));
            }
        }
    }

    // Game state
    data.points = get_int(root, "points");
    data.lives = get_int(root, "lives");
    data.sector = get_int(root, "sector");
    data.srScore = get_real(root, "srScore");
    data.hsScore = get_real(root, "hsScore");

    // Time state
    yyjson_val *tick_val = yyjson_obj_get(root, "tick");
    if (tick_val && yyjson_is_uint(tick_val)) {
        data.tick = static_cast<size_t>(yyjson_get_uint(tick_val));
    }

    yyjson_doc_free(doc);
    return Result<SavestateData>::Ok(data);
}

// ============================================================================
// SavestateManager Implementation
// ============================================================================

SavestateManager::SavestateManager(ServiceProvider *provider)
    : m_ServiceProvider(provider), m_GameInterface(nullptr), m_CacheValid(false) {
    // Resolve GameInterface (returns raw pointer, wrap in shared_ptr with no-op deleter since we don't own it)
    auto *gameInterface = m_ServiceProvider->Resolve<GameInterface>();
    if (gameInterface) {
        m_GameInterface = std::shared_ptr<GameInterface>(gameInterface, [](GameInterface *) {});
    }

    // Setup savestates directory
    m_SavestatesDir = "./BallanceTAS/savestates";

    // Create directory if it doesn't exist
    try {
        fs::create_directories(m_SavestatesDir);
    } catch (const fs::filesystem_error &e) {
        Log::Error("Failed to create savestates directory: %s", e.what());
    }
}

Result<void> SavestateManager::SaveState(const string &name) {
    return SaveState(name, "");
}

Result<void> SavestateManager::SaveState(const string &name, const string &description) {
    // Validate name
    auto validate_result = ValidateName(name);
    if (validate_result.IsError()) {
        return validate_result;
    }

    // Capture current state
    auto capture_result = CaptureState();
    if (capture_result.IsError()) {
        return Result<void>::Error(capture_result.GetError());
    }

    SavestateData data = capture_result.Unwrap();
    data.name = name;
    data.description = description;

    // Serialize to JSON
    string json = data.ToJson();

    // Write to file
    string filepath = GetSavestatePath(name);
    try {
        ofstream file(filepath);
        if (!file.is_open()) {
            return Result<void>::Error("Failed to open file for writing: " + filepath);
        }

        file << json;
        file.close();

        Log::Info("Savestate saved: %s", name.c_str());

        // Update cache
        m_StateCache[name] = data;
        m_CacheValid = true;

        return Result<void>::Ok();
    } catch (const exception &e) {
        return Result<void>::Error("Failed to write savestate file: " + string(e.what()));
    }
}

Result<void> SavestateManager::LoadState(const string &name) {
    // Validate name
    auto validateResult = ValidateName(name);
    if (validateResult.IsError()) {
        return validateResult;
    }

    // Check if file exists
    if (!StateExists(name)) {
        return Result<void>::Error("Savestate does not exist: " + name);
    }

    // Read file
    string filepath = GetSavestatePath(name);
    try {
        ifstream file(filepath);
        if (!file.is_open()) {
            return Result<void>::Error("Failed to open file for reading: " + filepath);
        }

        stringstream buffer;
        buffer << file.rdbuf();
        string json = buffer.str();
        file.close();

        // Deserialize
        auto dataResult = SavestateData::FromJson(json);
        if (dataResult.IsError()) {
            return Result<void>::Error("Failed to parse savestate: " + dataResult.GetError().message);
        }

        const SavestateData &data = dataResult.Unwrap();

        // Restore state
        auto restoreResult = RestoreState(data);
        if (restoreResult.IsError()) {
            return restoreResult;
        }

        Log::Info("Savestate loaded: %s", name.c_str());
        return Result<void>::Ok();
    } catch (const exception &e) {
        return Result<void>::Error("Failed to read savestate file: " + string(e.what()));
    }
}

Result<void> SavestateManager::DeleteState(const string &name) {
    // Validate name
    auto validateResult = ValidateName(name);
    if (validateResult.IsError()) {
        return validateResult;
    }

    string filepath = GetSavestatePath(name);

    try {
        if (fs::exists(filepath)) {
            fs::remove(filepath);
            Log::Info("Savestate deleted: %s", name.c_str());

            // Remove from cache
            m_StateCache.erase(name);

            return Result<void>::Ok();
        } else {
            return Result<void>::Error("Savestate does not exist: " + name);
        }
    } catch (const fs::filesystem_error &e) {
        return Result<void>::Error("Failed to delete savestate: " + string(e.what()));
    }
}

bool SavestateManager::StateExists(const string &name) const {
    string filepath = GetSavestatePath(name);
    return fs::exists(filepath);
}

Result<vector<string>> SavestateManager::ListStates() const {
    vector<string> states;

    try {
        if (!fs::exists(m_SavestatesDir)) {
            return Result<vector<string>>::Ok(states);
        }

        for (const auto &entry : fs::directory_iterator(m_SavestatesDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                states.push_back(entry.path().stem().string());
            }
        }

        return Result<vector<string>>::Ok(states);
    } catch (const fs::filesystem_error &e) {
        return Result<vector<string>>::Error("Failed to list savestates: " + string(e.what()));
    }
}

Result<SavestateData> SavestateManager::GetStateInfo(const string &name) const {
    // Check cache first
    if (m_CacheValid && m_StateCache.count(name) > 0) {
        return Result<SavestateData>::Ok(m_StateCache.at(name));
    }

    // Read from file
    if (!StateExists(name)) {
        return Result<SavestateData>::Error("Savestate does not exist: " + name);
    }

    string filepath = GetSavestatePath(name);
    try {
        ifstream file(filepath);
        if (!file.is_open()) {
            return Result<SavestateData>::Error("Failed to open file: " + filepath);
        }

        stringstream buffer;
        buffer << file.rdbuf();
        string json = buffer.str();
        file.close();

        auto data_result = SavestateData::FromJson(json);
        if (data_result.IsError()) {
            return Result<SavestateData>::Error("Failed to parse savestate: " + data_result.GetError().message);
        }

        const SavestateData &data = data_result.Unwrap();

        // Update cache
        m_StateCache[name] = data;

        return Result<SavestateData>::Ok(data);
    } catch (const exception &e) {
        return Result<SavestateData>::Error("Failed to read savestate: " + string(e.what()));
    }
}

string SavestateManager::GetSavestatesDirectory() const {
    return m_SavestatesDir;
}

// ============================================================================
// Private Methods
// ============================================================================

Result<SavestateData> SavestateManager::CaptureState() const {
    if (!m_GameInterface) {
        return Result<SavestateData>::Error("GameInterface not available");
    }

    SavestateData data;

    // Get current timestamp
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    data.timestamp = ss.str();

    // Get level info
    data.levelName = m_GameInterface->GetMapName();
    data.levelNumber = m_GameInterface->GetCurrentLevel();

    // Get ball entity
    CK3dEntity *ball = m_GameInterface->GetActiveBall();
    if (!ball) {
        return Result<SavestateData>::Error("Ball entity not available");
    }

    // Capture physics state
    VxVector pos;
    ball->GetPosition(&pos);
    data.position = pos;

    // Get physics object for velocity
    auto physics_obj = m_GameInterface->GetPhysicsObject(ball);
    if (physics_obj) {
        VxVector vel, angVel;
        physics_obj->GetVelocity(&vel, &angVel);
        data.velocity = vel;
        data.angularVelocity = angVel;
    } else {
        // Default to zero if physics not available
        data.velocity = VxVector(0, 0, 0);
        data.angularVelocity = VxVector(0, 0, 0);
    }

    // Get rotation
    VxQuaternion rot;
    ball->GetQuaternion(&rot);
    data.rotation = rot;

    // Capture RNG state
    data.rngState = CaptureRNGState();

    // Capture game state
    data.points = m_GameInterface->GetPoints();
    data.lives = m_GameInterface->GetLifeCount();
    data.sector = m_GameInterface->GetCurrentSector();
    data.srScore = m_GameInterface->GetSRScore();
    data.hsScore = static_cast<float>(m_GameInterface->GetHSScore());

    // Capture time state (TODO: implement GetCurrentTick in GameInterface)
    data.tick = 0; // Placeholder until GetCurrentTick is implemented

    return Result<SavestateData>::Ok(data);
}

Result<void> SavestateManager::RestoreState(const SavestateData &data) {
    if (!m_GameInterface) {
        return Result<void>::Error("GameInterface not available");
    }

    // Verify we're in the same level
    if (data.levelName != m_GameInterface->GetMapName()) {
        return Result<void>::Error("Cannot load savestate from different level");
    }

    // Get ball entity
    CK3dEntity *ball = m_GameInterface->GetActiveBall();
    if (!ball) {
        return Result<void>::Error("Ball entity not available");
    }

    // Restore physics state
    ball->SetPosition(&data.position);
    ball->SetQuaternion(&data.rotation);

    // Restore velocity
    auto physics_obj = m_GameInterface->GetPhysicsObject(ball);
    if (physics_obj) {
        // SetVelocity takes both linear and angular velocity as pointers
        VxVector linearVel = data.velocity;
        VxVector angularVel = data.angularVelocity;
        physics_obj->SetVelocity(&linearVel, &angularVel);
    }

    // Restore RNG state
    auto rng_result = RestoreRNGState(data.rngState);
    if (rng_result.IsError()) {
        Log::Warn("Failed to restore RNG state: %s", rng_result.GetError().message.c_str());
        // Continue anyway, RNG is not critical
    }

    if (!m_GameInterface->SetPoints(data.points)) {
        return Result<void>::Error("Failed to restore points");
    }

    if (!m_GameInterface->SetLifeCount(data.lives)) {
        return Result<void>::Error("Failed to restore life count");
    }

    if (!m_GameInterface->SetCurrentSector(data.sector)) {
        return Result<void>::Error("Failed to restore current sector");
    }

    if (!m_GameInterface->SetSRScore(data.srScore)) {
        return Result<void>::Error("Failed to restore SR score");
    }

    if (!m_GameInterface->SetHSScore(static_cast<int>(data.hsScore))) {
        return Result<void>::Error("Failed to restore HS score");
    }

    // Note: Time state (tick) is not restored as it would affect timing
    // Users can manually adjust if needed

    return Result<void>::Ok();
}

string SavestateManager::GetSavestatePath(const string &name) const {
    return m_SavestatesDir + "/" + name + ".json";
}

Result<void> SavestateManager::ValidateName(const string &name) const {
    if (name.empty()) {
        return Result<void>::Error("Savestate name cannot be empty");
    }

    // Check for invalid characters (only allow alphanumeric, underscore, hyphen)
    regex valid_pattern("^[a-zA-Z0-9_-]+$");
    if (!regex_match(name, valid_pattern)) {
        return Result<void>::Error("Savestate name contains invalid characters (only a-zA-Z0-9_- allowed)");
    }

    if (name.length() > 64) {
        return Result<void>::Error("Savestate name too long (max 64 characters)");
    }

    return Result<void>::Ok();
}

vector<uint32_t> SavestateManager::CaptureRNGState() const {
    // Capture RNG state from GameInterface
    RNGState state = m_GameInterface->GetRNGState();

    // Convert RNGState to vector<uint32_t>
    // RNGState contains: short id, short next_movement_check, int ivp_seed, int qh_seed
    vector<uint32_t> rng_data;
    rng_data.push_back(static_cast<uint32_t>(state.id));
    rng_data.push_back(static_cast<uint32_t>(state.next_movement_check));
    rng_data.push_back(static_cast<uint32_t>(state.ivp_seed));
    rng_data.push_back(static_cast<uint32_t>(state.qh_seed));

    return rng_data;
}

Result<void> SavestateManager::RestoreRNGState(const vector<uint32_t> &state) {
    if (state.empty()) {
        return Result<void>::Ok(); // Nothing to restore
    }

    if (state.size() != 4) {
        return Result<void>::Error("Invalid RNG state size: expected 4 values, got " + std::to_string(state.size()));
    }

    // Convert vector<uint32_t> back to RNGState
    RNGState rngState;
    rngState.id = static_cast<short>(state[0]);
    rngState.next_movement_check = static_cast<short>(state[1]);
    rngState.ivp_seed = static_cast<int>(state[2]);
    rngState.qh_seed = static_cast<int>(state[3]);

    // Restore RNG state using physics functions
    ivp_srand(rngState.ivp_seed);
    qh_srand(rngState.qh_seed);

    // Note: next_movement_check would need to be restored via IpionManager
    // This requires access to the IpionManager instance
    // For now, we restore the seed values which are the most critical

    Log::Info("Restored RNG state: ivp_seed=%d, qh_seed=%d", rngState.ivp_seed, rngState.qh_seed);

    return Result<void>::Ok();
}
