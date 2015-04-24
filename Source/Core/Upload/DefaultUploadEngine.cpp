/*

    Image Uploader -  free application for uploading images/files to the Internet

    Copyright 2007-2015 Sergey Svistunov (zenden2k@gmail.com)

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

*/

#include "DefaultUploadEngine.h"
#include "Core/3rdpart/codepages.h"
#include "Core/3rdpart/pcreplusplus.h"
#include "FileUploadTask.h"
#include "Core/Upload/UrlShorteningTask.h"
#include "Core/Utils/StringUtils.h"

CDefaultUploadEngine::CDefaultUploadEngine(ServerSync* serverSync) : CAbstractUploadEngine(serverSync)
{
	m_CurrentActionIndex = -1;
}

int CDefaultUploadEngine::doUpload(UploadTask* task, CIUUploadParams &params) {
	if ( task->type() == UploadTask::TypeFile ) {
		return doUploadFile(static_cast<FileUploadTask*>(task), params);
	} else if ( task->type() == UploadTask::TypeUrl  ) {
		return doUploadUrl(static_cast<UrlShorteningTask*>(task), params);
	} else {
		UploadError( ErrorInfo::mtError, "Upload task of type '" + task->toString() + "' is not supported", 0, false );
	}
	return 0;
}

bool CDefaultUploadEngine::doUploadFile(FileUploadTask* task, CIUUploadParams &params) {
	std::string fileName = task->getFileName();
	std::string displayName = task->getDisplayName();
	if ( fileName.empty() ) {
		UploadError( true, "Filename should not be empty!", 0 );
		return false;
	}

	if ( !m_UploadData ) {
		UploadError( true, "m_UploadData should not be NULL!", 0 );
		return false;
	}
	m_FileName = fileName;
	m_displayFileName = displayName;

	prepareUpload();
	std::string FileExt = IuCoreUtils::ExtractFileExt(displayName);
	m_Consts["_FILENAME"] = IuCoreUtils::ExtractFileName(displayName);
	std::string OnlyFname;
	OnlyFname = IuCoreUtils::ExtractFileNameNoExt(displayName);
	m_Consts["_FILENAMEWITHOUTEXT"] = OnlyFname;
	m_Consts["_FILEEXT"]            = FileExt;

	bool actionsExecuteResult = executeActions();
	if ( !actionsExecuteResult ) {
		return false;
	}

	Utf8String m_ThumbUrl    = ReplaceVars( m_UploadData->ThumbUrlTemplate ); 
	Utf8String m_ImageUrl    = ReplaceVars( m_UploadData->ImageUrlTemplate ); 
	Utf8String m_DownloadUrl = ReplaceVars( m_UploadData->DownloadUrlTemplate ); 

	params.ThumbUrl  = m_ThumbUrl;
	params.DirectUrl = m_ImageUrl;
	params.ViewUrl   = m_DownloadUrl;
	return true;
}

bool  CDefaultUploadEngine::doUploadUrl(UrlShorteningTask* task, CIUUploadParams &params) {
	prepareUpload();
	m_Consts["_ORIGINALURL"] = task->getUrl();
	bool actionsExecuteResult = executeActions();
	if ( !actionsExecuteResult ) {
		return false;
	}
	Utf8String m_ThumbUrl    = ReplaceVars( m_UploadData->ThumbUrlTemplate ); 
	Utf8String m_ImageUrl    = ReplaceVars( m_UploadData->ImageUrlTemplate ); 
	Utf8String m_DownloadUrl = ReplaceVars( m_UploadData->DownloadUrlTemplate ); 

	params.ThumbUrl  = m_ThumbUrl;
	params.DirectUrl = m_ImageUrl;
	params.ViewUrl   = m_DownloadUrl;
	if ( m_ImageUrl.empty() ) {
		UploadError( ErrorInfo::mtError, "Empty result", 0, false );
		return false;
	}
	return true;
}

void CDefaultUploadEngine::prepareUpload() {
	m_Vars.clear();
	if ( m_UploadData->NeedAuthorization ) {
		li = m_ServersSettings.authData;
		if ( li.DoAuth ) {
			m_Consts["_LOGIN"]    = li.Login;
			m_Consts["_PASSWORD"] = li.Password;
		}
	}

	int n = rand() % (256 * 256);
	m_Consts["_RAND16BITS"]         = IuCoreUtils::toString(n);
	m_Consts["_THUMBWIDTH"]         = IuCoreUtils::toString( m_ThumbnailWidth );
}

bool CDefaultUploadEngine::executeActions() {
	m_NetworkClient->setTreatErrorsAsWarnings(true);
	for (size_t i = 0; i < m_UploadData->Actions.size(); i++) {
		m_UploadData->Actions[i].NumOfTries = 0;
		bool ActionRes = false;
		do {
			if ( needStop() ) {
				return false;
			}
			ActionRes = DoAction( m_UploadData->Actions[i] );
			m_UploadData->Actions[i].NumOfTries ++;
			if (needStop())
				return false;

			if (!ActionRes ) {
				// Prepare error string which will be displayed in Log Window
				std::string ErrorStr = m_ErrorReason; 
				ErrorType errorType;
				ErrorInfo::MessageType mt = ErrorInfo::mtWarning;
				if (!m_ErrorReason.empty()) {
					ErrorStr += m_ErrorReason;
				}

				if (m_UploadData->Actions[i].NumOfTries == m_UploadData->Actions[i].RetryLimit) {
					errorType = etActionRetriesLimitReached;
				}
				else {
					errorType = etActionRepeating;
					ErrorStr += "retrying... ";
					ErrorStr += "(" + IuCoreUtils::toString(m_UploadData->Actions[i].NumOfTries + 1)
						+ " of " + IuCoreUtils::toString(m_UploadData->Actions[i].RetryLimit) + ")";
				}
				UploadError( mt == ErrorInfo::mtError, ErrorStr, 0, false );
			}
		}
		while (m_UploadData->Actions[i].NumOfTries < m_UploadData->Actions[i].RetryLimit && !ActionRes);
		if ( !ActionRes ) {
			return false;
		}
	}
	return true;
}



bool CDefaultUploadEngine::DoUploadAction(UploadAction& Action, bool bUpload)
{
    try {

        AddQueryPostParams(Action);

		m_NetworkClient->setUrl(Action.Url);

		if ( bUpload ) {
			if (Action.Type == "put") {
				m_NetworkClient->setMethod( "PUT" );
				m_NetworkClient->doUpload( m_FileName, "" );
			} else {
				m_NetworkClient->doUploadMultipartData();
			}
		} else {
			m_NetworkClient->doPost();
		}

		return ReadServerResponse(Action);
	}
    catch (std::exception ex) {
        std::cerr<<ex.what()<<std::endl;
        return false;
	}
	return true;
}

bool CDefaultUploadEngine::DoGetAction(UploadAction& Action)
{
	bool Result = false;

	try {
		m_NetworkClient->doGet( Action.Url );
		if (needStop()) {
			return false;
		}
		Result = ReadServerResponse(Action);
	}
	catch (...)
	{
		return false;
	}
	return Result;
}

bool reg_single_match(const std::string pattern, const std::string& text, std::string& res)
{
	pcrepp::Pcre reg(pattern, "imc"); // Case insensitive match
	if ( reg.search(text) == true ) {
		if ( reg.matches() > 0 ) {
			res = reg.get_match(0);
		}
		return true;
	}
	
	return false;
}

bool CDefaultUploadEngine::ParseAnswer(UploadAction& Action, std::string& Body)
{
	if ( !Action.RegExp.empty() ) {
		std::string codePage;
		if ( reg_single_match( "text/html;\\s+charset=([\\w-]+)", Body, codePage ) ) {
			IuCoreUtils::ConvertToUtf8( Body, codePage );
		}
		if ( m_UploadData->Debug ) {
			DebugMessage( Body, true );
		}

		try {
			pcrepp::Pcre reg( Action.RegExp, "imc" );

			std::string DebugVars = "Regex: " + Action.RegExp + "\r\n\r\n";
			if ( reg.search(Body) ) {
				reg.matches();
				if (Action.Variables.empty()) {
					DebugVars += "Variables list is empty!\r\n";
				}

				for (size_t i = 0; i < Action.Variables.size(); i++) {
					ActionVariable& v = Action.Variables[i];
					std::string temp;
					temp = reg.get_match(v.nIndex);
					if (!v.Name.empty() && v.Name[0] == '_')
						m_Consts[v.Name] = temp;
					m_Vars[v.Name] = temp;
					DebugVars += v.Name + " = " + temp + "\r\n";
				}
			}
			else {
				if (m_UploadData->Debug) {
					DebugVars += "NO MATCHES FOUND!";
					DebugMessage( DebugVars );
				}
				UploadError( false, "Cannot obtain the necessary information from server response.", &Action );
				return false; // ERROR! Current action failed!
			}
			if (m_UploadData->Debug) {
				DebugMessage( DebugVars );
			}
		}
		catch (const std::exception& e) {
			UploadError( true, std::string("Regular expression error:") + e.what(), &Action, false );
		}
	}
	return true;
}

bool CDefaultUploadEngine::DoAction(UploadAction& Action)
{
	bool Result = true;

	m_CurrentActionIndex = Action.Index;
	if (Action.OnlyOnce)
	{
		if (m_PerformedActions[Action.Index] == true)
			return true;
	}

	if (!Action.Description.empty())
		SetStatus(stUserDescription, Action.Description);
	else
	{
		if (Action.Type == "upload")
			SetStatus(stUploading);
		else
		if (Action.Type == "login" && (m_UploadData->NeedAuthorization && li.DoAuth))
			SetStatus(stAuthorization);
		else
		{
			SetStatus(stPerformingAction);
		}
	}

	UploadAction Current = Action;
	std::string temp = Current.Url;
	Current.Url = ReplaceVars(temp);
	if (m_UploadData->Debug)
		DebugMessage("\r\nType:" + Action.Type + "\r\nURL: " + Current.Url);

	if (Action.Type == "upload")
		Result = DoUploadAction(Current, true);
	else
	if (Action.Type == "put")
		Result = DoUploadAction(Current, true);
	else
	if (Action.Type == "post")
		Result = DoUploadAction(Current, false);
	else
	if (Action.Type == "login")
	{
		if (m_UploadData->NeedAuthorization && li.DoAuth)
			Result = DoUploadAction(Current, false);
	}
	else
	if (Action.Type == "get")
		Result = DoGetAction(Current);

	if (Action.OnlyOnce)
	{
		if (Result)
			m_PerformedActions[Action.Index] = true;
	}

	if (Action.IgnoreErrors)
		return true;
	else
		return Result;
}

bool CDefaultUploadEngine::ReadServerResponse(UploadAction& Action)
{

	bool Result = false;
	bool Exit = false;


	int StatusCode = m_NetworkClient->responseCode();

	if (!(StatusCode >= 200 && StatusCode <= 299) && !(StatusCode >= 300 && StatusCode <= 304))
	{
		std::string error;
		if (m_NetworkClient->getCurlResult() != CURLE_OK)
		{
			error = "Curl error: " + m_NetworkClient->getCurlResultString();
		}
		else
		{
			error += "Server response code: " + IuCoreUtils::toString(StatusCode) + "\r\n";
			error += m_NetworkClient->errorString();
		}
		if (!StatusCode)
			StatusCode = m_NetworkClient->getCurlResult();
		UploadError(false, error, &Action);
		return false;
	}

	std::string Refresh = m_NetworkClient->responseHeaderByName("Refresh");

	if (!Refresh.empty()) // Redirecting to URL
	{
		std::string redirectUrl;
		if (reg_single_match("url=(\\S+)", Refresh, redirectUrl))
		{
			UploadAction Redirect = Action;
			Redirect.Url = redirectUrl;
			Redirect.Referer = Action.Url;
			Redirect.Type = "get";
			Result = DoGetAction(Redirect);
			Exit = true;
		}
	}

	if (!Exit)
	{
		std::string answer = m_NetworkClient->responseBody();
		Result =  ParseAnswer(Action, answer);

		if (!Result)
			UploadError(false, "Empty response!", &Action);
	}
	return Result;
}

void CDefaultUploadEngine::AddQueryPostParams(UploadAction& Action)
{
	m_NetworkClient->setReferer(Action.Referer.empty() ? Action.Url : ReplaceVars(Action.Referer));

	std::string Txt = Action.PostParams;
	int len = Txt.length();
	if (len)
	{
		if (Txt[len - 1] != ';')
		{
			Txt += ";";
		}
	}

	std::string _Post = "Post Request to URL: " + Action.Url + "\r\n";

	pcrepp::Pcre reg("(.*?)=(.*?[^\\x5c]{0,1});", "imc");

	std::string str = Txt;

	size_t pos = 0;
    while (pos < str.length())
	{

		if ( reg.search(str, pos))
		{

			std::string VarName = reg[0];
			std::string VarValue = reg[1];
			pos = reg.get_match_end() + 1;

			if (!VarName.length())
				continue;

			std::string NewValue = VarValue;
			NewValue = IuCoreUtils::StrReplace(NewValue, "\\;", ";");

			std::string NewName = VarName;

			NewName = ReplaceVars(NewName);
			std::string vv = NewName;

			if (NewValue == "%filename%")
			{
				_Post += NewName + " = ** FILE CONTENTS ** \r\n";
				m_NetworkClient->addQueryParamFile(NewName, m_FileName,
				                                    IuCoreUtils::ExtractFileName(
				                                       m_displayFileName), IuCoreUtils::GetFileMimeType(m_FileName));
			}
			else
			{
				NewValue = ReplaceVars(NewValue);
				_Post += NewName + " = " + NewValue + "\r\n";
				m_NetworkClient->addQueryParam(NewName, NewValue);
			}

		}
		else
			break;
	}

	//m_NetworkClient->addQueryHeader("Origin", "http://radikal.ru/");
	//m_NetworkClient->addQueryHeader("User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/36.0.1985.125 Safari/537.36");
	//m_NetworkClient->addQueryHeader("User-Agent", "RADIKALCLIENT");

	if (m_UploadData->Debug)
		DebugMessage(_Post);
}

std::string CDefaultUploadEngine::ReplaceVars(const std::string& Text)
{
	std::string Result =  Text;

	pcrepp::Pcre reg("\\$\\(([A-z0-9_|]*?)\\)", "imc");
	std::string str = (Text);
	size_t pos = 0;
	while (pos <= str.length())
	{
		if ( reg.search(str, pos))
		{
			pos = reg.get_match_end() + 1;
			std::string vv = reg[0];
			std::string varName = vv;
			std::vector<std::string> tokens;
			IuStringUtils::Split(vv, "|", tokens, -1);
			if ( tokens.size() ) {
				varName = tokens[0];
			}
			std::string value;

			if (!vv.empty() && vv[0] == '_') {
				value = m_Consts[varName];
			} else {
				value = m_Vars[varName];
			}
			for ( int i = 1; i < tokens.size(); i++ ) {
				std::string modifier = tokens[i];
				if ( modifier == "urlencode" ) {
					value = m_NetworkClient->urlEncode(value);
				}
			}
			
			Result = IuCoreUtils::StrReplace(Result, std::string("$(") + vv + std::string(")"), value);
		}
		else
			break;
	}
	return Result;
}

bool CDefaultUploadEngine::needStop()
{
	bool shouldStop = false;
	if (onNeedStop)
		shouldStop = onNeedStop();  // delegate call
	return shouldStop;
}

void CDefaultUploadEngine::SetStatus(StatusType status, std::string param)
{
	if (onStatusChanged)
		onStatusChanged(status, m_CurrentActionIndex, param);
}

int CDefaultUploadEngine::RetryLimit()
{
	return m_UploadData->RetryLimit;
}

void CDefaultUploadEngine::UploadError(bool error, const std::string errorStr, UploadAction* m_CurrentAction,
                                       bool writeToBuffer )
{
	m_LastError.ServerName = m_UploadData->Name;

	if (m_CurrentAction)
	{
		m_LastError.ActionIndex = m_CurrentAction->Index + 1;
		m_LastError.Url = m_CurrentAction->Url;
		m_LastError.RetryIndex = m_CurrentAction->NumOfTries + 1;
	}

	if (!m_LastError.error.empty())
		m_LastError.error += "\n";

	m_LastError.error += errorStr;

	if (!writeToBuffer)
	{
		m_LastError.messageType = error ? (ErrorInfo::mtError) : (ErrorInfo::mtWarning);
		m_LastError.errorType = etUserError;
		m_LastError.sender = "CDefaultUploadEngine";
		ErrorMessage(m_LastError);
		m_LastError.Clear();
	}
}
