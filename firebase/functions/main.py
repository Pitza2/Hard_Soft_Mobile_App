from firebase_functions import https_fn
from firebase_functions.options import set_global_options
from firebase_admin import initialize_app, firestore, messaging
import json

# Limit concurrent instances
set_global_options(max_instances=5)

# Initialize Firebase Admin
initialize_app()


@https_fn.on_request()
def send_notification(req: https_fn.Request) -> https_fn.Response:
    try:
        # Create Firestore client INSIDE function
        db = firestore.client()

        # Allow only POST
        if req.method != "POST":
            return https_fn.Response(
                "Method not allowed",
                status=405
            )

        # Parse JSON body
        body = req.get_json(silent=True)

        if not body:
            return https_fn.Response(
                "Invalid JSON body",
                status=400
            )

        email = body.get("email")
        message_text = body.get("message")
        timestamp = body.get("timestamp")

        if not email or not message_text:
            return https_fn.Response(
                "Missing email or message",
                status=400
            )

        # Query users collection
        user_id = email.strip() 

        user_doc_ref = db.collection("users").document(user_id)
        user_doc = user_doc_ref.get()

        if not user_doc.exists:
            return https_fn.Response(f"No user found with email/ID: {user_id}",status=404)

        # Success!
        user_data = user_doc.to_dict()
        # Get FCM token
        fcm_token = user_data.get("fcmToken")

        if not fcm_token:
            return https_fn.Response(
                "User does not have an fcmToken",
                status=404
            )

        # Create notification
        message = messaging.Message(
            notification=messaging.Notification(
                title="New Notification",
                body=message_text,
            ),
            token=fcm_token,
            data={
                "email": email,
                "message": message_text,
                "timestamp": str(timestamp) if timestamp else ""
            }
        )

        # Send notification
        response = messaging.send(message)

        return https_fn.Response(
            json.dumps({
                "success": True,
                "messageId": response
            }),
            status=200,
            content_type="application/json"
        )

    except Exception as e:
        return https_fn.Response(
            json.dumps({
                "success": False,
                "error": str(e)
            }),
            status=500,
            content_type="application/json"
        )