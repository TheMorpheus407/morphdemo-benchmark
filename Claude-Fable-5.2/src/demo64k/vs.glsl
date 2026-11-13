void main() { gl_Position = vec4(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 4. - 1., 0, 1); }
