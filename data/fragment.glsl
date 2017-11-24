#version 430

in vec3 textureCoords;
in vec4 fragPosition;
in vec3 fragEye;
in vec3 fragNormal;
in vec4 light;

out vec4 FragmentColour;

uniform float Ns;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform vec3 Ke;
uniform float Ni;
uniform float d;
uniform float illum;
uniform vec3 lightPos;

uniform int ao_bool;
uniform int color_bool;

uniform sampler2D tex;
uniform sampler2D ao;

void main(void) {

     vec3 light_ambient = Ka;
     vec3 color = vec3(0.5, 0.5, 0.5);
     if(ao_bool == 1) {
          vec3 aocoord = texture(ao, textureCoords.xy).xyz;
          light_ambient = vec3(Ka.x*aocoord.x, Ka.y*aocoord.y, Ka.z*aocoord.z);
     }
     if(color_bool == 1) {
          color = texture(tex, textureCoords.xy).xyz;
     }

     vec3 diffuse_color = Kd;
     vec3 specular_color = Ks;
     float specular_power = Ns;

     // normalize eye and normal vectors
     vec4 v = fragPosition;
     vec3 eye = normalize(fragEye);
     vec3 n = normalize(fragNormal);
     vec4 l = normalize(light);

     float diffuse_intensity = clamp(max(dot(n, l.xyz), 0), 0.0, 1.0);

     vec3 half_angle = normalize(l.xyz + eye);

     float specular_weight = clamp(dot(half_angle, n), 0.0, 1.0);

     vec3 lite;
     vec3 a = light_ambient;
     vec3 d = (diffuse_intensity * diffuse_color);
     vec3 s = specular_color * pow(specular_weight, specular_power);

     lite = a + d + s;
     vec3 colour = clamp(vec3(color.x*lite.x, color.y*lite.y, color.z*lite.z), 0.0, 1.0);

     FragmentColour = vec4(colour, 1.0);

}
