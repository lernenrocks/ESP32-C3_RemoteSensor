#include "HX711Sensor.h"
#include "InternalStorage.h"

// Key names within a sensor's NVS namespace — constant across all
// instances, safe as file-scope (unlike the namespace itself, which must
// be per-instance, see nvsNamespace()).
char prefOffset[] = "offset";
char prefScale[] = "scale";

void HX711Sensor::nvsNamespace(char *out, size_t len) const
{
    snprintf(out, len, "sensor_%d", _pId);
}

HX711Sensor::HX711Sensor(int dout, int sck, uint8_t pId) : _pId(pId)
{
    _scale.begin(dout, sck);
    _scale.set_scale();
    char ns[NVS_NAMESPACE_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    InternalStorage::Session session(ns, true);
    long offset;
    float scale;
    if (InternalStorage::readLong(prefOffset, offset) && InternalStorage::readFloat(prefScale, scale))
    {
        _scale.set_offset(offset);
        _scale.set_scale(scale);
    }
}
bool HX711Sensor::isValid()
{
    return _scale.is_ready();
}

bool HX711Sensor::getCalibrationJson(char *buffer, size_t len)
{
    const char info[] =
        "[{\"instruction\":\"Remove all weight, then confirm\",\"key\":\"offset\"},"
        "{\"instruction\":\"Add known weight\",\"key\":\"ref_weight\",\"ref\":\"ref_weight\",\"unit\":\"g\"}]";
    size_t written = snprintf(buffer, len, "%s", info);
    return written < len;
}
bool HX711Sensor::getCalibrationValuesJson(char *buffer, size_t len)
{
    size_t written = snprintf(buffer, len, "{\"offset\":%ld,\"scale\":%g}", _scale.get_offset(), _scale.get_scale());
    return written < len;
}
void HX711Sensor::readRaw(float &buffer)
{
    //! Increase averaging samples if values fluctuate; decrease if MainUnit TCP timeout is hit
    buffer = static_cast<float>(_scale.get_units(10));
}
int HX711Sensor::getPrecision()
{
    return 0;
}
const char* HX711Sensor::getUnit()
{
    return "g";
}
bool HX711Sensor::calibrate(const JsonObjectConst data)
{
    if (data["offset"].isNull() || data["ref_weight"].isNull())
    {
        return false;
    }
    long offset = data["offset"];
    float refWeight = data["ref_weight"];
    if (refWeight <= 0)
    {
        return false;
    }
    char ns[NVS_NAMESPACE_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    InternalStorage::Session session(ns, false);
    InternalStorage::writeLong(prefOffset, offset);
    _scale.set_offset(offset);
    float newScale = _scale.get_value(10)/refWeight;
    InternalStorage::writeFloat(prefScale, newScale);
    _scale.set_scale(newScale);
    return true;
}

void HX711Sensor::reset()
{
    char ns[NVS_NAMESPACE_LEN] = {};
    nvsNamespace(ns, sizeof(ns));
    InternalStorage::Session session(ns, false);
    InternalStorage::writeLong(prefOffset, 0);
    InternalStorage::writeFloat(prefScale, 1.0f);
    _scale.set_scale(1.0f);
    _scale.set_offset(0);
}
