#!/bin/bash
# Fix Error -> ErrorData
sed -i 's/TSharedPtr<FJsonObject> Error;/TSharedPtr<FJsonObject> ErrorData;/' Source/YesUeMcp/Public/Protocol/McpTypes.h
sed -i 's/Response.Error =/Response.ErrorData =/g' Source/YesUeMcp/Private/Protocol/McpTypes.cpp
sed -i 's/else if (Error.IsValid())/else if (ErrorData.IsValid())/' Source/YesUeMcp/Private/Protocol/McpTypes.cpp
sed -i 's/JsonObject->SetObjectField(TEXT("error"), Error);/JsonObject->SetObjectField(TEXT("error"), ErrorData);/' Source/YesUeMcp/Private/Protocol/McpTypes.cpp

# Fix FThreadSafeBool IsSet() -> operator bool
sed -i 's/CancellationToken->IsSet()/(*CancellationToken)/' Source/YesUeMcp/Public/Tools/McpToolBase.h

# Fix ANY_PACKAGE -> nullptr
sed -i 's/ANY_PACKAGE/nullptr/g' Source/YesUeMcpEditor/Private/Tools/Asset/AssetSearchTool.cpp

echo "Fixes applied"
