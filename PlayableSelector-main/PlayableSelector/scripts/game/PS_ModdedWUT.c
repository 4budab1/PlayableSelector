
modded class SCR_CallsignGroupComponent
{
	override bool IsUniqueRoleInUse(int roleToCheck)
	{
		if (!m_Group) return true;
		
		return super.IsUniqueRoleInUse(roleToCheck);
	}
}
