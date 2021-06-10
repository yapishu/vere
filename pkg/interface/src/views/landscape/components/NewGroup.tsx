import {
  Box, Col,

  ManagedCheckboxField as Checkbox, ManagedTextInputField as Input, Text
} from '@tlon/indigo-react';
import { createGroup, Enc, GroupPolicy } from '@urbit/api';
import { Form, Formik, FormikHelpers } from 'formik';
import React, { ReactElement, useCallback } from 'react';
import { useHistory } from 'react-router-dom';
import * as Yup from 'yup';
import { useWaitForProps } from '~/logic/lib/useWaitForProps';
import { stringToSymbol } from '~/logic/lib/util';
import useGroupState from '~/logic/state/group';
import useMetadataState from '~/logic/state/metadata';
import { AsyncButton } from '~/views/components/AsyncButton';
import airlock from '~/logic/api';

const formSchema = Yup.object({
  title: Yup.string().required('Group must have a name'),
  description: Yup.string(),
  isPrivate: Yup.boolean()
});

interface FormSchema {
  title: string;
  description: string;
  isPrivate: boolean;
}

export function NewGroup(): ReactElement {
  const history = useHistory();
  const initialValues: FormSchema = {
    title: '',
    description: '',
    isPrivate: false
  };

  const groups = useGroupState(state => state.groups);
  const associations = useMetadataState(state => state.associations);
  const waiter = useWaitForProps({ groups, associations });

  const onSubmit = useCallback(
    async (values: FormSchema, actions: FormikHelpers<FormSchema>) => {
      try {
        const { title, description, isPrivate } = values;
        const name = stringToSymbol(title.trim());
        const policy: Enc<GroupPolicy> = isPrivate
          ? {
              invite: {
                pending: []
              }
            }
          : {
              open: {
                banRanks: [],
                banned: []
              }
            };
        await airlock.thread(createGroup(name, policy, title, description));
        const path = `/ship/~${window.ship}/${name}`;
        await waiter((p) => {
          return path in p.groups && path in p.associations.groups;
        });

        actions.setStatus({ success: null });
        history.push(`/~landscape${path}`);
      } catch (e) {
        console.error(e);
        actions.setStatus({ error: e.message });
      }
    },
    [waiter, history]
  );

  return (
    <>
      <Col overflowY="auto" p={3}>
        <Box mb={3}>
          <Text fontWeight="bold">New Group</Text>
        </Box>
        <Formik
          validationSchema={formSchema}
          initialValues={initialValues}
          onSubmit={onSubmit}
        >
          <Form>
            <Col gapY={4}>
              <Input
                id="title"
                label="Name"
                caption="Provide a name for your group"
                placeholder="eg. My Channel"
              />
              <Input
                id="description"
                label="Description"
                caption="What's your group about?"
                placeholder="Group description"
              />
              <Checkbox
                id="isPrivate"
                label="Private Group"
                caption="Anyone can join a public group. A private group is only joinable by invite."
              />
              <AsyncButton>Create Group</AsyncButton>
            </Col>
          </Form>
        </Formik>
      </Col>
    </>
  );
}
