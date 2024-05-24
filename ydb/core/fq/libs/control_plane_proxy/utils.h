#pragma once

#include <ydb/public/api/protos/draft/fq.pb.h>

namespace NFq {

inline TString GetServiceAccountId(const FederatedQuery::IamAuth& auth) {
    return auth.has_service_account()
            ? auth.service_account().id()
            : TString{};
}

template<typename T>
TString ExtractServiceAccountIdImpl2(const T& setting) {
    switch (setting.connection_case()) {
    case FederatedQuery::ConnectionSetting::kYdbDatabase: {
        return GetServiceAccountId(setting.ydb_database().auth());
    }
    case FederatedQuery::ConnectionSetting::kDataStreams: {
        return GetServiceAccountId(setting.data_streams().auth());
    }
    case FederatedQuery::ConnectionSetting::kObjectStorage: {
        return GetServiceAccountId(setting.object_storage().auth());
    }
    case FederatedQuery::ConnectionSetting::kMonitoring: {
        return GetServiceAccountId(setting.monitoring().auth());
    }
    case FederatedQuery::ConnectionSetting::kClickhouseCluster: {
        return GetServiceAccountId(setting.clickhouse_cluster().auth());
    }
    // Do not replace with default. Adding a new connection should cause a compilation error
    case FederatedQuery::ConnectionSetting::CONNECTION_NOT_SET:
    break;
    }
    return {};
}

inline TString ExtractServiceAccountId(const FederatedQuery::TestConnectionRequest& c) {
    return ExtractServiceAccountIdImpl2(c.setting());
}

inline TString ExtractServiceAccountId(const FederatedQuery::CreateConnectionRequest&  c) {
    return ExtractServiceAccountIdImpl2(c.content().setting());
}

inline TString ExtractServiceAccountId(const FederatedQuery::ModifyConnectionRequest&  c) {
    return ExtractServiceAccountIdImpl2(c.content().setting());
}

template<typename T>
TString ExtractServiceAccountId(const T&) {
    return {};
}

} // namespace NFq
