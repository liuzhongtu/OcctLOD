#pragma once
#include <AIS_TextLabel.hxx>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <TCollection_AsciiString.hxx>

class SceneHud
{
public:
	SceneHud(const opencascade::handle<AIS_InteractiveContext>& ctx,
		const opencascade::handle<V3d_View>& view);

	void Update(const TCollection_AsciiString& text);

private:
	void ensureDisplayed_();

private:
	opencascade::handle<AIS_InteractiveContext> m_ctx;
	opencascade::handle<V3d_View>               m_view;
	opencascade::handle<AIS_TextLabel>          m_label;
};