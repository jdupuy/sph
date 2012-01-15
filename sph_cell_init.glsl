#version 420 core

#ifdef _VERTEX_

layout(location=0) out ivec4 oData;

void main()
{
	oData = ivec4(-1);
}

#endif // _VERTEX_

