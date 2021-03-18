#version 450 core

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

#define FLT_MAX  3.402823466e+38F



layout(binding = 1) uniform texture2D SceneTexture;

layout(UPDATE_FREQ_NONE, binding = 4) uniform sampler nearestSampler;
layout(UPDATE_FREQ_NONE, binding = 5) uniform sampler bilinearSampler;



layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main()
{	
	if(renderMode == 0)
	{
		outColor = texture(sampler2D(SceneTexture, bilinearSampler), uv);
		outColor = outColor/(outColor+vec4(1.0));
		outColor = pow(outColor, vec4(1.0/2.2));
		outColor.w = 1.0;
		return;
	}
}