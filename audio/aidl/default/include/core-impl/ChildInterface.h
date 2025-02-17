/*
 * Copyright (C) 2023 The Android Open Source Project
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

#pragma once

#include <memory>
#include <utility>

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder_platform.h>
#include <system/thread_defs.h>

namespace aidl::android::hardware::audio::core {

// Helper used for interfaces that require a persistent instance. We hold them via a strong
// pointer. The binder token is retained for a call to 'setMinSchedulerPolicy'.
template <class C>
struct ChildInterface : private std::pair<std::shared_ptr<C>, ndk::SpAIBinder> {
    ChildInterface() = default;
    ChildInterface& operator=(const std::shared_ptr<C>& c) {
        return operator=(std::shared_ptr<C>(c));
    }
    ChildInterface& operator=(std::shared_ptr<C>&& c) {
        this->first = std::move(c);
        return *this;
    }
    explicit operator bool() const { return !!this->first; }
    C& operator*() const { return *(this->first); }
    C* operator->() const { return this->first; }
    std::shared_ptr<C> getPtr() { return this->first; }
    // Use 'getInstance' when returning the interface instance.
    std::shared_ptr<C> getInstance() {
        (void)getBinder();
        return this->first;
    }
    AIBinder* getBinder() {
        if (this->second.get() == nullptr) {
            const auto binder = this->second = this->first->asBinder();
            AIBinder_setMinSchedulerPolicy(binder.get(), SCHED_NORMAL, ANDROID_PRIORITY_AUDIO);
            AIBinder_setInheritRt(binder.get(), true);
        }
        return this->second.get();
    }
};

}  // namespace aidl::android::hardware::audio::core
