package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.common.SessionUtils;
import com.catramms.backing.newWorkflow.PushContent;
import com.catramms.backing.newWorkflow.WorkflowIssue;
import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONObject;
import org.primefaces.event.FileUploadEvent;

import java.io.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.TimeZone;

public class EmailNotificationProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(EmailNotificationProperties.class);

    private String emailAddress;
    private String subject;
    private String message;

    public EmailNotificationProperties(String positionX, String positionY,
                                       int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "Email-Notification" + "-icon.png", "Task", "Email-Notification");
    }

    public EmailNotificationProperties clone()
    {
        EmailNotificationProperties emailNotificationProperties = new EmailNotificationProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        emailNotificationProperties.setEmailAddress(emailAddress);
        emailNotificationProperties.setSubject(subject);
        emailNotificationProperties.setMessage(message);

        return emailNotificationProperties;
    }

    public void setData(EmailNotificationProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setEmailAddress(workflowProperties.getEmailAddress());
        setSubject(workflowProperties.getSubject());
        setMessage(workflowProperties.getMessage());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            setLabel(jsonWorkflowElement.getString("Label"));

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("EmailAddress") && !joParameters.getString("EmailAddress").equalsIgnoreCase(""))
                setEmailAddress(joParameters.getString("EmailAddress"));
            if (joParameters.has("Subject") && !joParameters.getString("Subject").equalsIgnoreCase(""))
                setSubject(joParameters.getString("Subject"));
            if (joParameters.has("Message") && !joParameters.getString("Message").equalsIgnoreCase(""))
                setMessage(joParameters.getString("Message"));
        }
        catch (Exception e)
        {
            mLogger.error("WorkflowProperties:setData failed, exception: " + e);
        }
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            jsonWorkflowElement.put("Type", super.getType());

            JSONObject joParameters = new JSONObject();
            jsonWorkflowElement.put("Parameters", joParameters);

            mLogger.info("task.getType: " + super.getType());

            if (super.getLabel() != null && !super.getLabel().equalsIgnoreCase(""))
                jsonWorkflowElement.put("Label", super.getLabel());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel("");
                workflowIssue.setFieldName("Label");
                workflowIssue.setTaskType(super.getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getEmailAddress() != null && !getEmailAddress().equalsIgnoreCase(""))
                joParameters.put("EmailAddress", getEmailAddress());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("EmailAddress");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getSubject() != null && !getSubject().equalsIgnoreCase(""))
                joParameters.put("Subject", getSubject());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Subject");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            if (getMessage() != null && !getMessage().equalsIgnoreCase(""))
                joParameters.put("Message", getMessage());
            else
            {
                WorkflowIssue workflowIssue = new WorkflowIssue();
                workflowIssue.setLabel(getLabel());
                workflowIssue.setFieldName("Message");
                workflowIssue.setTaskType(getType());
                workflowIssue.setIssue("The field is not initialized");

                ingestionData.getWorkflowIssueList().add(workflowIssue);
            }

            super.addEventsPropertiesToJson(jsonWorkflowElement, ingestionData);
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public String getEmailAddress() {
        return emailAddress;
    }

    public void setEmailAddress(String emailAddress) {
        this.emailAddress = emailAddress;
    }

    public String getSubject() {
        return subject;
    }

    public void setSubject(String subject) {
        this.subject = subject;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }
}
