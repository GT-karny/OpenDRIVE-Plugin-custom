#include "Public/EditorMode/SOpenDriveEditorModeWidget.h"
#include "Public/OpenDriveEditor.h"
#include "Public/EditorMode/OpenDriveEditorMode.h"
#include "Public/EditorMode/SOpenDriveEditorModeWidget.h"

void SOpenDRIVEEditorModeWidget::Construct(const FArguments& InArgs)
{
	_fontInfoPtr = MakeShareable(new FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), 12));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 30.f, 0.f, 0.f)
		[
			ConstructButtons(InArgs)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.f, 30.f, 20.f, 0.f)
		[
			ConstructRoadGenerationParameters(InArgs)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.f, 30.f, 20.f, 0.f)
		[
			ConstructLaneInfoBox(InArgs)
		]
	];
}

TSharedRef<SBorder> SOpenDRIVEEditorModeWidget::ConstructLaneInfoBox(const FArguments& InArgs)
{
	_roadIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("RoadId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected road's Id.")));

	_junctionIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("JunctionId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected road's junction's Id.")));

	_laneTypeTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Lane type : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected lane's type.")));

	_laneIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("LaneId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected lane's Id.")));

	_successorIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Successor Id : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The road's successorId.")));

	_predecessorIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Predecessor Id : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The road's predecessorId.")));

	TSharedRef<SBorder> border =
		SNew(SBorder)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.f, 10.f, 10.f, 0.f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 10.f, 0.f, 0.f)
					[
						_roadIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 0.f)
					[
						_successorIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 0.f)
					[
						_predecessorIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 10.f)
					[
						_junctionIdTextPtr.ToSharedRef()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.f, 20.f, 10.f, 10.f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 10.f, 0.f, 0.f)
					[
						_laneIdTextPtr.ToSharedRef()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 10.f)
					[
						_laneTypeTextPtr.ToSharedRef()
					]
				]
			]

		];
	return border;
}

TSharedRef<SHorizontalBox> SOpenDRIVEEditorModeWidget::ConstructButtons(const FArguments& InArgs)
{
	// Reset Button
	TSharedPtr<SButton> resetButton = SNew(SButton).Text(FText::FromString("Reset"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::Reset).IsEnabled(this, &SOpenDRIVEEditorModeWidget::IsLoaded)
		.ToolTipText(FText::FromString(TEXT("Resets currently drawn roads.")));

	StaticCast<STextBlock&>(resetButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Generate Button
	TSharedPtr<SButton> generateButton = SNew(SButton).Text(FText::FromString("Generate"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::Generate).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Draws roads (will reset currently drawn roads).")));

	StaticCast<STextBlock&>(generateButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Generate Splines Button
	TSharedPtr<SButton> generateSplinesButton = SNew(SButton).Text(FText::FromString("Gen Splines"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::GenerateLaneSplines).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Generates persistent spline actors for all lanes.")));

	StaticCast<STextBlock&>(generateSplinesButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	/* 
	 * Layout Strategy:
	 * Since the return type signature is TSharedRef<SHorizontalBox>, we MUST return a HorizontalBox.
	 * We can either put all buttons in one row, or put a VerticalBox inside the HorizontalBox.
	 * Let's try putting all 3 buttons in one row for simplicity and to match the signature.
	 */

	TSharedRef<SHorizontalBox> horBox = 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(20, 0, 0, 0).FillWidth(0.33f)
		[
			resetButton.ToSharedRef()
		]
		+ SHorizontalBox::Slot().Padding(10, 0, 0, 0).FillWidth(0.33f)
		[
			generateButton.ToSharedRef()
		]
		+ SHorizontalBox::Slot().Padding(10, 0, 20, 0).FillWidth(0.33f)
		[
			generateSplinesButton.ToSharedRef()
		];

	return horBox;
}

TSharedRef<SBorder> SOpenDRIVEEditorModeWidget::ConstructRoadGenerationParameters(const FArguments& InArgs)
{
	_offsetTextPtr = SNew(STextBlock).Justification(ETextJustify::Center)
		.Text(FText::FromString("ZOffset : " + FString::FormatAsNumber(GetEdMode()->GetRoadOffset())))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("Roads' ZOffset : by defaults, the road's network will be drawn at Z=20.\nSo if you already have static meshes to represent your roads, higher that value to avoid texture flickering.")));

	_stepTextPtr = SNew(STextBlock).Justification(ETextJustify::Center)
		.Text(FText::FromString("Step : " + FString::FormatAsNumber(GetEdMode()->GetStep())))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("Lower this value for a more precise draw (and less performances !).")));

	TSharedRef<SSlider> OffsetSlider = SNew(SSlider).MinValue(0.f).MaxValue(80.f)
		.Value(GetEdMode()->GetRoadOffset())
		.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnOffsetValueChanged);

	TSharedRef<SSlider> StepSlider = SNew(SSlider).MinValue(1.f).MaxValue(10.f)
		.Value(GetEdMode()->GetStep())
		.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnStepValueChanged);

	_showArrowsCheckBox = SNew(SCheckBox)
		.IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnCheckStateChanged);

	_splineGenModeOptions.Add(MakeShareable(new FString("Center")));
	_splineGenModeOptions.Add(MakeShareable(new FString("Inside")));
	_splineGenModeOptions.Add(MakeShareable(new FString("Outside")));

	_splineGenModeComboBox = SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&_splineGenModeOptions)
		.OnGenerateWidget(this, &SOpenDRIVEEditorModeWidget::MakeSplineResampleModeWidget)
		.OnSelectionChanged(this, &SOpenDRIVEEditorModeWidget::OnSplineResampleModeChanged)
		[
			SNew(STextBlock).Text_Lambda([this]()
			{
				if (_splineGenModeComboBox.IsValid() && _splineGenModeComboBox->GetSelectedItem().IsValid())
				{
					return FText::FromString(*_splineGenModeComboBox->GetSelectedItem());
				}
				return FText::FromString("Center");
			})
			.Font(*_fontInfoPtr)
		];
	
	// Set initial selection based on mode
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode CurrentMode = GetEdMode()->GetSplineGenerationMode();
	if (_splineGenModeOptions.IsValidIndex((int)CurrentMode))
	{
		_splineGenModeComboBox->SetSelectedItem(_splineGenModeOptions[(int)CurrentMode]);
	}

	TSharedRef<SBorder> border = SNew(SBorder)
		[
			SNew(SVerticalBox)
			// General Generation Filters
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString("General Generation Filters")).Font(*_fontInfoPtr).Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnGenerateRoadsCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Generate Roads")) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnGenerateJunctionsCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Generate Junctions")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
			// Lane Filters
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString("Lane Generation Filters")).Font(*_fontInfoPtr).Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnDrivingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Driving")) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnSidewalkLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Sidewalk")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnBikingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Biking")) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnParkingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Parking")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnShoulderLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Shoulder")) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnRestrictedLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Restricted")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnMedianLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Median")) ] ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnOtherLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Other")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnReferenceLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Reference")) ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]

			// Spline Generation Mode
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString("Spline Generation Reference")).Font(*_fontInfoPtr).Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f).HAlign(HAlign_Center)
			[
				_splineGenModeComboBox.ToSharedRef()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]



			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 10.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Show arrows")))
				.Font(*_fontInfoPtr)
				.Justification(ETextJustify::Center)
				.ToolTipText(FText::FromString(TEXT("Tick the checkbox to see the roads' directions.")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 5.f, 0.f, 0.f)
			.HAlign(HAlign_Center)
			[
				_showArrowsCheckBox.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 10.f, 0.f, 0.f)
			[
				_offsetTextPtr.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 5.f, 5.f, 0.f)
			[
				OffsetSlider
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 5.f, 0.f, 0.f)
			[
				_stepTextPtr.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f, 5.f, 5.f, 10.f)
			[
				StepSlider
			]
			
		];

	return border;
}

FOpenDRIVEEditorMode* SOpenDRIVEEditorModeWidget::GetEdMode() const
{
	return (FOpenDRIVEEditorMode*)GLevelEditorModeTools().GetActiveMode(FOpenDRIVEEditorMode::EM_RoadMode);
}

FReply SOpenDRIVEEditorModeWidget::Reset()
{
	GetEdMode()->ResetRoadsArray();
	_showArrowsCheckBox.Get()->SetIsChecked(ECheckBoxState::Unchecked);
	return FReply::Handled();
}

bool SOpenDRIVEEditorModeWidget::IsLoaded() const
{
	return (GetEdMode()->GetHasBeenLoaded() && CheckIfInEditorMode());
}

bool SOpenDRIVEEditorModeWidget::CheckIfInEditorMode() const
{
	return !(GEditor->IsPlayingSessionInEditor());
}

FReply SOpenDRIVEEditorModeWidget::Generate()
{
	GetEdMode()->Generate();
	_showArrowsCheckBox.Get()->SetIsChecked(ECheckBoxState::Unchecked);
	return FReply::Handled();
}

void SOpenDRIVEEditorModeWidget::UpdateLaneInfo(AOpenDriveEditorLane* lane_)
{
	_roadIdTextPtr.Get()->SetText(FText::FromString("Road Id : " + FString::FromInt(lane_->GetRoadId())));

	_junctionIdTextPtr.Get()->SetText(FText::FromString("Junction Id : " + FString::FromInt(lane_->GetJunctionId())));

	_laneTypeTextPtr.Get()->SetText(FText::FromString("Lane type : " + lane_->GetLaneType()));

	_laneIdTextPtr.Get()->SetText(FText::FromString("Lane Id : " + FString::FromInt(lane_->GetLaneId())));

	_successorIdTextPtr.Get()->SetText(FText::FromString("Successor Id : " + FString::FromInt(lane_->GetSuccessorId())));

	_predecessorIdTextPtr.Get()->SetText(FText::FromString("Predecessor Id : " + FString::FromInt(lane_->GetPredecessorId())));
}

void SOpenDRIVEEditorModeWidget::SetOffset(const FText &newOffset_)
{
	FString string = newOffset_.ToString();

	float offset = string.IsNumeric() ? FCString::Atof(*string) : 10.f;

	GetEdMode()->SetRoadOffset(offset);
}

void SOpenDRIVEEditorModeWidget::OnCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetRoadsArrowsVisibilityInEditor(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnOffsetValueChanged(float value)
{
	_offsetTextPtr->SetText(FText::FromString("Road's Zoffset : " + FString::FormatAsNumber(value)));
	GetEdMode()->SetRoadOffset(value);
}

void SOpenDRIVEEditorModeWidget::OnStepValueChanged(float value)
{
	_stepTextPtr->SetText(FText::FromString("Step : " + FString::FormatAsNumber(value)));
	GetEdMode()->SetStep(value);
}

FReply SOpenDRIVEEditorModeWidget::GenerateLaneSplines()
{
	GetEdMode()->GenerateLaneSplines();
	return FReply::Handled();
}

void SOpenDRIVEEditorModeWidget::OnDrivingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateDrivingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnSidewalkLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateSidewalkLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnGenerateRoadsCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateRoads(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnGenerateJunctionsCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateJunctions(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnBikingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateBikingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnParkingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateParkingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnShoulderLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateShoulderLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnRestrictedLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateRestrictedLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnMedianLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateMedianLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnOtherLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateOtherLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnReferenceLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateReferenceLane(state == ECheckBoxState::Checked);
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::MakeSplineResampleModeWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption)).Font(*_fontInfoPtr);
}

void SOpenDRIVEEditorModeWidget::OnSplineResampleModeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type Type)
{
	if (!NewValue.IsValid()) return;

	if (*NewValue == "Center")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Center);
	}
	else if (*NewValue == "Inside")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Inside);
	}
	else if (*NewValue == "Outside")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Outside);
	}
}

