modded class SCR_ServerFullDialog : SCR_ConfigurableDialogUi
{
	override protected void UpdateDetailIcons()
	{
		if (!m_Room)
			return;
		if (!m_Widgets)
			return;

		if (m_Widgets.m_wDetailIcon_PasswordProtected)
			m_Widgets.m_wDetailIcon_PasswordProtected.SetVisible(m_Room.PasswordProtected());
		if (m_Widgets.m_wDetailIcon_CrossPlatform)
			m_Widgets.m_wDetailIcon_CrossPlatform.SetVisible(m_Room.IsCrossPlatform());
		if (m_Widgets.m_wDetailIcon_Modded)
			m_Widgets.m_wDetailIcon_Modded.SetVisible(m_Room.IsModded());
	}
}
