<Shader Name="HorizonBasedAO">
	<Include>Standard</Include>
	
	<PROGRAM>
		<UseAttribute>vbSurface</UseAttribute>
		<UseUniform>cbPerFrame</UseUniform>
		<UseUniform>cbPerLight</UseUniform>
		<UseTexture>tGrpPostProc</UseTexture>
		<Uniform Name="cbHBAo" Slot="3" ShareMode="PerMaterial">
			<Element Name="DepthParam" Type="float4" Default="3.33333,3.33233"></Element> 
			<Element Name="FocalLen" Type="float4" Default="1.0,1.0,1.0,1.0"></Element>
			<Element Name="Radius" Type="float4" Default="0.35,0.1225,2.857"></Element>
			<Element Name="NumStepDirContrast" Type="float4" Default="8,16,1.25"></Element>
			<Element Name="AttenTanBias" Type="float4" Default="1.0,30"></Element>
			<Element Name="BlurRadiusFallOffSharp" Type="float4" Default="7,0.03125,2089.796"></Element>
		</Uniform>
		<Texture>
			<Element Slot="9" Filter="MinMagMipLinear">PrePassOutput</Element>
		</Texture>
		<FileName>HorizonBasedAO</FileName>
		<VertexEntry>VS</VertexEntry>
	</PROGRAM>
	
	<SubShader>
		<RenderType>Overlay</RenderType> 
		<Pass>
			<LightMode>PostProcess</LightMode>
			<PROGRAM> 
				<PixelEntry>PSAO</PixelEntry>
			</PROGRAM>
			<GrabPass>PassOutAo</GrabPass>
		</Pass>
		<Pass>
			<UseGrab>PassOutAo{bind_slot:9}</UseGrab>
			<LightMode>PostProcess</LightMode>
			<PROGRAM>
				<PixelEntry>PSBlurX</PixelEntry>
			</PROGRAM>
			<GrabPass>PassOutBlurX</GrabPass>
		</Pass>
		<Pass>
			<UseGrab>PassOutBlurX{bind_slot:9}</UseGrab>
			<LightMode>PostProcess</LightMode>
			<PROGRAM>
				<PixelEntry>PSBlurY</PixelEntry>
			</PROGRAM>
		</Pass>
	</SubShader>
</Shader>