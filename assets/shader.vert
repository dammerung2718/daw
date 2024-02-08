#version 450

layout(location = 0) in vec2 pos;

layout(push_constant) uniform constants {
  vec2 resolution;
} PushConstants;

void main() {
  vec2 uv = pos / PushConstants.resolution - 1;
  gl_Position = vec4(uv, 0.0, 1.0);
}
