/*
 * Copyright 2019 Google
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

#ifndef FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_API_DOCUMENT_REFERENCE_H_
#define FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_API_DOCUMENT_REFERENCE_H_

#if !defined(__OBJC__)
#error "This header only supports Objective-C++"
#endif  // !defined(__OBJC__)

#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>

#include "Firestore/core/src/firebase/firestore/api/document_snapshot.h"
#include "Firestore/core/src/firebase/firestore/api/listener_registration.h"
#include "Firestore/core/src/firebase/firestore/core/listen_options.h"
#include "Firestore/core/src/firebase/firestore/core/user_data.h"
#include "Firestore/core/src/firebase/firestore/model/document_key.h"
#include "Firestore/core/src/firebase/firestore/model/resource_path.h"
#include "Firestore/core/src/firebase/firestore/util/status.h"
#include "Firestore/core/src/firebase/firestore/util/statusor_callback.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace firestore {
namespace api {

class Firestore;
enum class Source;

class DocumentReference {
 public:
  DocumentReference() = default;
  DocumentReference(model::ResourcePath path,
                    std::shared_ptr<Firestore> firestore);
  DocumentReference(model::DocumentKey document_key,
                    std::shared_ptr<Firestore> firestore)
      : firestore_{firestore}, key_{std::move(document_key)} {
  }

  size_t Hash() const;

  const std::shared_ptr<Firestore>& firestore() const {
    return firestore_;
  }
  const model::DocumentKey& key() const {
    return key_;
  }

  const std::string& document_id() const;

  // TODO(varconst) uncomment when core API CollectionReference is implemented.
  // CollectionReference Parent() const;

  std::string Path() const;

  // TODO(varconst) uncomment when core API CollectionReference is implemented.
  // CollectionReference GetCollectionReference(
  //     const std::string& collection_path) const;

  void SetData(core::ParsedSetData&& setData, util::StatusCallback callback);

  void UpdateData(core::ParsedUpdateData&& updateData,
                  util::StatusCallback callback);

  void DeleteDocument(util::StatusCallback callback);

  void GetDocument(Source source, DocumentSnapshot::Listener&& callback);

  ListenerRegistration AddSnapshotListener(
      core::ListenOptions options, DocumentSnapshot::Listener&& listener);

 private:
  std::shared_ptr<Firestore> firestore_;
  model::DocumentKey key_;
};

bool operator==(const DocumentReference& lhs, const DocumentReference& rhs);

}  // namespace api
}  // namespace firestore
}  // namespace firebase

NS_ASSUME_NONNULL_END

#endif  // FIRESTORE_CORE_SRC_FIREBASE_FIRESTORE_API_DOCUMENT_REFERENCE_H_
