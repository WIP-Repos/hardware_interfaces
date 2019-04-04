/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "VmsUtils.h"

#include <common/include/vhal_v2_0/VehicleUtils.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {
namespace vms {

static constexpr int kMessageIndex = toInt(VmsBaseMessageIntegerValuesIndex::MESSAGE_TYPE);
static constexpr int kMessageTypeSize = 1;
static constexpr int kPublisherIdSize = 1;
static constexpr int kLayerNumberSize = 1;
static constexpr int kLayerSize = 3;
static constexpr int kLayerAndPublisherSize = 4;
static constexpr int kPublisherIdIndex =
        toInt(VmsPublisherInformationIntegerValuesIndex::PUBLISHER_ID);
static constexpr int kSubscriptionStateSequenceNumberIndex =
        toInt(VmsSubscriptionsStateIntegerValuesIndex::SEQUENCE_NUMBER);
static constexpr int kAvailabilitySequenceNumberIndex =
        toInt(VmsAvailabilityStateIntegerValuesIndex::SEQUENCE_NUMBER);

// TODO(aditin): We should extend the VmsMessageType enum to include a first and
// last, which would prevent breakages in this API. However, for all of the
// functions in this module, we only need to guarantee that the message type is
// between SUBSCRIBE and PUBLISHER_ID_RESPONSE.
static constexpr int kFirstMessageType = toInt(VmsMessageType::SUBSCRIBE);
static constexpr int kLastMessageType = toInt(VmsMessageType::PUBLISHER_ID_RESPONSE);

std::unique_ptr<VehiclePropValue> createBaseVmsMessage(size_t message_size) {
    auto result = createVehiclePropValue(VehiclePropertyType::INT32, message_size);
    result->prop = toInt(VehicleProperty::VEHICLE_MAP_SERVICE);
    result->areaId = toInt(VehicleArea::GLOBAL);
    return result;
}

std::unique_ptr<VehiclePropValue> createSubscribeMessage(const VmsLayer& layer) {
    auto result = createBaseVmsMessage(kMessageTypeSize + kLayerSize);
    result->value.int32Values = hidl_vec<int32_t>{toInt(VmsMessageType::SUBSCRIBE), layer.type,
                                                  layer.subtype, layer.version};
    return result;
}

std::unique_ptr<VehiclePropValue> createSubscribeToPublisherMessage(
    const VmsLayerAndPublisher& layer_publisher) {
    auto result = createBaseVmsMessage(kMessageTypeSize + kLayerAndPublisherSize);
    result->value.int32Values = hidl_vec<int32_t>{
        toInt(VmsMessageType::SUBSCRIBE_TO_PUBLISHER), layer_publisher.layer.type,
        layer_publisher.layer.subtype, layer_publisher.layer.version, layer_publisher.publisher_id};
    return result;
}

std::unique_ptr<VehiclePropValue> createUnsubscribeMessage(const VmsLayer& layer) {
    auto result = createBaseVmsMessage(kMessageTypeSize + kLayerSize);
    result->value.int32Values = hidl_vec<int32_t>{toInt(VmsMessageType::UNSUBSCRIBE), layer.type,
                                                  layer.subtype, layer.version};
    return result;
}

std::unique_ptr<VehiclePropValue> createUnsubscribeToPublisherMessage(
    const VmsLayerAndPublisher& layer_publisher) {
    auto result = createBaseVmsMessage(kMessageTypeSize + kLayerAndPublisherSize);
    result->value.int32Values = hidl_vec<int32_t>{
        toInt(VmsMessageType::UNSUBSCRIBE_TO_PUBLISHER), layer_publisher.layer.type,
        layer_publisher.layer.subtype, layer_publisher.layer.version, layer_publisher.publisher_id};
    return result;
}

std::unique_ptr<VehiclePropValue> createOfferingMessage(const VmsOffers& offers) {
    int message_size = kMessageTypeSize + kPublisherIdSize + kLayerNumberSize;
    for (const auto& offer : offers.offerings) {
        message_size += kLayerSize + kLayerNumberSize + (offer.dependencies.size() * kLayerSize);
    }
    auto result = createBaseVmsMessage(message_size);

    std::vector<int32_t> offerings = {toInt(VmsMessageType::OFFERING), offers.publisher_id,
                                      static_cast<int>(offers.offerings.size())};
    for (const auto& offer : offers.offerings) {
        std::vector<int32_t> layer_vector = {offer.layer.type, offer.layer.subtype,
                                             offer.layer.version,
                                             static_cast<int32_t>(offer.dependencies.size())};
        for (const auto& dependency : offer.dependencies) {
            std::vector<int32_t> dependency_layer = {dependency.type, dependency.subtype,
                                                     dependency.version};
            layer_vector.insert(layer_vector.end(), dependency_layer.begin(),
                                dependency_layer.end());
        }
        offerings.insert(offerings.end(), layer_vector.begin(), layer_vector.end());
    }
    result->value.int32Values = offerings;
    return result;
}

std::unique_ptr<VehiclePropValue> createAvailabilityRequest() {
    auto result = createBaseVmsMessage(kMessageTypeSize);
    result->value.int32Values = hidl_vec<int32_t>{
        toInt(VmsMessageType::AVAILABILITY_REQUEST),
    };
    return result;
}

std::unique_ptr<VehiclePropValue> createSubscriptionsRequest() {
    auto result = createBaseVmsMessage(kMessageTypeSize);
    result->value.int32Values = hidl_vec<int32_t>{
        toInt(VmsMessageType::SUBSCRIPTIONS_REQUEST),
    };
    return result;
}

std::unique_ptr<VehiclePropValue> createDataMessage(const std::string& bytes) {
    auto result = createBaseVmsMessage(kMessageTypeSize);
    result->value.int32Values = hidl_vec<int32_t>{toInt(VmsMessageType::DATA)};
    result->value.bytes = std::vector<uint8_t>(bytes.begin(), bytes.end());
    return result;
}

bool isValidVmsProperty(const VehiclePropValue& value) {
    return (value.prop == toInt(VehicleProperty::VEHICLE_MAP_SERVICE));
}

bool isValidVmsMessageType(const VehiclePropValue& value) {
    return (value.value.int32Values.size() > 0 &&
            value.value.int32Values[kMessageIndex] >= kFirstMessageType &&
            value.value.int32Values[kMessageIndex] <= kLastMessageType);
}

bool isValidVmsMessage(const VehiclePropValue& value) {
    return (isValidVmsProperty(value) && isValidVmsMessageType(value));
}

VmsMessageType parseMessageType(const VehiclePropValue& value) {
    return static_cast<VmsMessageType>(value.value.int32Values[kMessageIndex]);
}

std::string parseData(const VehiclePropValue& value) {
    if (isValidVmsMessage(value) && parseMessageType(value) == VmsMessageType::DATA &&
        value.value.bytes.size() > 0) {
        return std::string(value.value.bytes.begin(), value.value.bytes.end());
    } else {
        return std::string();
    }
}

std::unique_ptr<VehiclePropValue> createPublisherIdRequest(
        const std::string& vms_provider_description) {
    auto result = createBaseVmsMessage(kMessageTypeSize);
    result->value.int32Values = hidl_vec<int32_t>{
            toInt(VmsMessageType::PUBLISHER_ID_REQUEST),
    };
    result->value.bytes =
            std::vector<uint8_t>(vms_provider_description.begin(), vms_provider_description.end());
    return result;
}

int32_t parsePublisherIdResponse(const VehiclePropValue& publisher_id_response) {
    if (isValidVmsMessage(publisher_id_response) &&
        parseMessageType(publisher_id_response) == VmsMessageType::PUBLISHER_ID_RESPONSE &&
        publisher_id_response.value.int32Values.size() > kPublisherIdIndex) {
        return publisher_id_response.value.int32Values[kPublisherIdIndex];
    }
    return -1;
}

bool isSequenceNumberNewer(const VehiclePropValue& subscription_change,
                           const int last_seen_sequence_number) {
    return (isValidVmsMessage(subscription_change) &&
            parseMessageType(subscription_change) == VmsMessageType::SUBSCRIPTIONS_CHANGE &&
            subscription_change.value.int32Values.size() > kSubscriptionStateSequenceNumberIndex &&
            subscription_change.value.int32Values[kSubscriptionStateSequenceNumberIndex] >
                    last_seen_sequence_number);
}

int32_t getSequenceNumberForSubscriptionsState(const VehiclePropValue& subscription_change) {
    if (isValidVmsMessage(subscription_change) &&
        parseMessageType(subscription_change) == VmsMessageType::SUBSCRIPTIONS_CHANGE &&
        subscription_change.value.int32Values.size() > kSubscriptionStateSequenceNumberIndex) {
        return subscription_change.value.int32Values[kSubscriptionStateSequenceNumberIndex];
    }
    return -1;
}

std::vector<VmsLayer> getSubscribedLayers(const VehiclePropValue& subscription_change,
                                          const VmsOffers& offers) {
    if (isValidVmsMessage(subscription_change) &&
        parseMessageType(subscription_change) == VmsMessageType::SUBSCRIPTIONS_CHANGE &&
        subscription_change.value.int32Values.size() > kSubscriptionStateSequenceNumberIndex) {
        const int32_t num_of_layers = subscription_change.value.int32Values[toInt(
                VmsSubscriptionsStateIntegerValuesIndex::NUMBER_OF_LAYERS)];
        const int32_t num_of_associated_layers = subscription_change.value.int32Values[toInt(
                VmsSubscriptionsStateIntegerValuesIndex ::NUMBER_OF_ASSOCIATED_LAYERS)];

        std::unordered_set<VmsLayer, VmsLayer::VmsLayerHashFunction> offered_layers;
        for (const auto& offer : offers.offerings) {
            offered_layers.insert(offer.layer);
        }
        std::vector<VmsLayer> subscribed_layers;

        int current_index = toInt(VmsSubscriptionsStateIntegerValuesIndex::SUBSCRIPTIONS_START);
        // Add all subscribed layers which are offered by the current publisher.
        for (int i = 0; i < num_of_layers; i++) {
            VmsLayer layer = VmsLayer(subscription_change.value.int32Values[current_index],
                                      subscription_change.value.int32Values[current_index + 1],
                                      subscription_change.value.int32Values[current_index + 2]);
            if (offered_layers.find(layer) != offered_layers.end()) {
                subscribed_layers.push_back(layer);
            }
            current_index += kLayerSize;
        }
        // Add all subscribed associated layers which are offered by the current publisher.
        // For this, we need to check if the associated layer has a publisher ID which is
        // same as that of the current publisher.
        for (int i = 0; i < num_of_associated_layers; i++) {
            VmsLayer layer = VmsLayer(subscription_change.value.int32Values[current_index],
                                      subscription_change.value.int32Values[current_index + 1],
                                      subscription_change.value.int32Values[current_index + 2]);
            current_index += kLayerSize;
            if (offered_layers.find(layer) != offered_layers.end()) {
                int32_t num_of_publisher_ids = subscription_change.value.int32Values[current_index];
                current_index++;
                for (int j = 0; j < num_of_publisher_ids; j++) {
                    if (subscription_change.value.int32Values[current_index] ==
                        offers.publisher_id) {
                        subscribed_layers.push_back(layer);
                    }
                    current_index++;
                }
            }
        }
        return subscribed_layers;
    }
    return {};
}

bool hasServiceNewlyStarted(const VehiclePropValue& availability_change) {
    return (isValidVmsMessage(availability_change) &&
            parseMessageType(availability_change) == VmsMessageType::AVAILABILITY_CHANGE &&
            availability_change.value.int32Values.size() > kAvailabilitySequenceNumberIndex &&
            availability_change.value.int32Values[kAvailabilitySequenceNumberIndex] == 0);
}

}  // namespace vms
}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
