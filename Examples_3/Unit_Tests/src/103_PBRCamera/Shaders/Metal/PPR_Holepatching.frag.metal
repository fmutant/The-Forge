/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include <metal_stdlib>
using namespace metal;
struct VSOutput {
	float4 Position [[position]];
	float2 uv;
};

struct CSData {
    sampler nearestSampler                  [[id(0)]];
    sampler bilinearSampler                 [[id(1)]];
    texture2d<float> SceneTexture           [[id(2)]];
};

fragment float4 stageMain(
    VSOutput input [[stage_in]],
    constant CSData& csData [[buffer(UPDATE_FREQ_NONE)]]
)
{	
	float4 outColor;

	outColor = csData.SceneTexture.sample(csData.bilinearSampler, input.uv);
	outColor = outColor / (outColor + float4(1.0f));

	float gammaCorr = 1.0f / 2.2f;

	outColor.r = pow(outColor.r, gammaCorr);
	outColor.g = pow(outColor.g, gammaCorr);
	outColor.b = pow(outColor.b, gammaCorr);
	outColor.w = 1.0;
	return outColor;
}
