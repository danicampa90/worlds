uniform sampler2D gry_texture;
uniform float gry_width;
uniform float gry_height;

float step_w = 1.0/gry_width;
float step_h = 1.0/gry_height;

void main(void)
{
  
   vec4 sum = vec4(0.0);
   
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(-step_w, -step_h)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(0.0, -step_h)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(step_w, -step_h)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(-step_w, 0.0)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(0.0, 0.0)) * 9.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(step_w, 0.0)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(-step_w, step_h)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(0.0, step_h)) * -1.0;
			sum += texture2D(gry_texture, gl_TexCoord[0].st + vec2(step_w, step_h)) * -1.0;
					
   gl_FragColor = sum;
}
