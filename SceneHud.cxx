#include "SceneHud.hxx"
#include <Prs3d_TextAspect.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Quantity_Color.hxx>

SceneHud::SceneHud(const opencascade::handle<AIS_InteractiveContext>& ctx,
	const opencascade::handle<V3d_View>& view)
	: m_ctx(ctx), m_view(view)
{
	m_label = new AIS_TextLabel();
	m_label->SetColor(Quantity_NOC_WHITE);

	Handle(Prs3d_TextAspect) ta = new Prs3d_TextAspect();
	ta->SetHeight(24.0);
	m_label->Attributes()->SetTextAspect(ta);

	// ¹Ì¶¨ÔÚÓÒÏÂ½Ç
	Handle(Graphic3d_TransformPers) tp =
		new Graphic3d_TransformPers(Graphic3d_TMF_2d, Aspect_TOTP_RIGHT_LOWER, Graphic3d_Vec2i(300, 60));
	m_label->SetTransformPersistence(tp);

	ensureDisplayed_();
}

void SceneHud::ensureDisplayed_()
{
	if (m_ctx.IsNull() || m_label.IsNull()) return;
	if (!m_ctx->IsDisplayed(m_label))
		m_ctx->Display(m_label, Standard_False);
}

void SceneHud::Update(const TCollection_AsciiString& text)
{
	if (m_label.IsNull()) return;
	m_label->SetText(text);
	m_label->SetColor(Quantity_NOC_WHITE);
	m_ctx->Redisplay(m_label, Standard_False, Standard_True);
}