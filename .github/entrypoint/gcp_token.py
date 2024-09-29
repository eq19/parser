import urllib

import google.auth.transport.requests
import google.oauth2.id_token

def make_authorized_get_request(endpoint, audience):
    """
    make_authorized_get_request makes a GET request to the specified HTTP endpoint
    by authenticating with the ID token obtained from the google-auth client library
    using the specified audience value.
    """

    # Cloud Functions uses your function's URL as the `audience` value
    # audience = https://project-region-projectid.cloudfunctions.net/myFunction
    audience = "https://us-central1-feedmapping.cloudfunctions.net/function"

    # For Cloud Functions, `endpoint` and `audience` should be equal
    # Ref: https://cloud.google.com/functions/docs/securing/authenticating
    endpoint = audience
    req = urllib.request.Request(endpoint)

    auth_req = google.auth.transport.requests.Request()
    id_token = google.oauth2.id_token.fetch_id_token(auth_req, audience)

    req.add_header("Authorization", f"Bearer {id_token}")
    response = urllib.request.urlopen(req)

    return response.read()
