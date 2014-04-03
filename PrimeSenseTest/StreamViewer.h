#pragma once

#include "OpenGL/gl2dcanvas.h"

class StreamViewer : public GL2DCanvas
{
public:
	StreamViewer(QWidget* parent = 0);
	~StreamViewer(void);

	void bindStreamData(const unsigned char* data);
	QSize sizeHint() const {
		return QSize(640, 480);
	}

protected:
	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);

private:
	GLuint textureId;
};

